#include "app/world/ally_announcements.h"

#include <QCoreApplication>
#include <QLatin1String>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

#include "game/session/session_context.h"
#include "game/systems/alliance_board.h"
#include "game/systems/economy/marketplace_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"

namespace App::World {

namespace {

using Game::Mission::CommanderMessageFact;
using Game::Mission::CommanderMessageTrigger;
using Game::Systems::AllyAppealFollowUp;
using Game::Systems::AllyAppealKind;
using Game::Systems::AllyCallVerdict;
using Game::Systems::AllyTributeVerdict;

struct Context {
  Game::Session::SessionContext& session;
  int local;
  const AllyAnnouncementSink& sink;

  [[nodiscard]] auto name_of(int owner_id) const -> QString {
    return owner_display_name(&session, owner_id);
  }
  [[nodiscard]] auto friendly(int owner_id) const -> bool {
    return is_friendly_commander(&session, local, owner_id);
  }
};

void announce_ally_to_ally_tribute(const Context& ctx,
                                   const Game::Systems::AllyTributeAnswer& answer,
                                   const QString& what) {
  if (!ctx.friendly(answer.requester) || !ctx.friendly(answer.giver)) {
    return;
  }
  const QString asker = ctx.name_of(answer.requester);
  const QString giver = ctx.name_of(answer.giver);
  const bool granted = answer.verdict == AllyTributeVerdict::Granted ||
                       answer.verdict == AllyTributeVerdict::Partial;
  ctx.sink.exchange(
      granted ? QCoreApplication::translate("GameEngine",
                                            "%1 asked %2 for %3 %4 and received %5.")
                    .arg(asker, giver)
                    .arg(answer.requested)
                    .arg(what)
                    .arg(answer.granted)
              : QCoreApplication::translate("GameEngine",
                                            "%1 asked %2 for %3 %4; %2 refused.")
                    .arg(asker, giver)
                    .arg(answer.requested)
                    .arg(what),
      granted);
}

void announce_reply_to_local_request(const Context& ctx,
                                     const Game::Systems::AllyTributeAnswer& answer,
                                     const QString& key,
                                     const QString& what) {
  const QString ally = ctx.name_of(answer.giver);
  CommanderMessageFact reply{.subject_owner_id = ctx.local,
                             .actor_owner_id = answer.giver,
                             .amount = answer.granted,
                             .resource = key};
  switch (answer.verdict) {
  case AllyTributeVerdict::Granted:
    ctx.sink.exchange(QCoreApplication::translate("GameEngine", "%1 sends you %2 %3.")
                          .arg(ally)
                          .arg(answer.granted)
                          .arg(what),
                      true);
    reply.trigger = CommanderMessageTrigger::RequestGranted;
    reply.reason = QStringLiteral("full");
    break;
  case AllyTributeVerdict::Partial:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine",
                                    "%1 can spare only %2 of the %3 %4 you asked for.")
            .arg(ally)
            .arg(answer.granted)
            .arg(answer.requested)
            .arg(what),
        true);
    reply.trigger = CommanderMessageTrigger::RequestGranted;
    reply.reason = QStringLiteral("partial");
    break;
  case AllyTributeVerdict::RefusedShort:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine", "%1 has no %2 to spare.")
            .arg(ally, what),
        false);
    reply.trigger = CommanderMessageTrigger::RequestRefused;
    reply.reason = QStringLiteral("short");
    reply.amount = answer.requested;
    break;
  case AllyTributeVerdict::RefusedStingy:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine", "%1 refuses to part with any %2.")
            .arg(ally, what),
        false);
    reply.trigger = CommanderMessageTrigger::RequestRefused;
    reply.reason = QStringLiteral("stingy");
    reply.amount = answer.requested;
    break;
  case AllyTributeVerdict::Sent:
    return;
  }
  ctx.sink.commander_fact(reply);
}

void announce_tribute_answer(const Context& ctx,
                             const Game::Systems::AllyTributeAnswer& answer) {
  const QString key = QLatin1String(Game::Systems::resource_type_key(answer.resource));
  const QString what = ally_resource_word(key);
  if (answer.verdict == AllyTributeVerdict::Sent) {
    if (answer.giver != ctx.local || answer.granted <= 0) {
      return;
    }
    ctx.sink.exchange(QCoreApplication::translate("GameEngine", "Sent %1 %2 to %3.")
                          .arg(answer.granted)
                          .arg(what, ctx.name_of(answer.requester)),
                      true);
    ctx.sink.commander_fact({.trigger = CommanderMessageTrigger::GiftReceived,
                             .subject_owner_id = answer.requester,
                             .actor_owner_id = ctx.local,
                             .amount = answer.granted,
                             .resource = key});
    return;
  }
  if (answer.requester != ctx.local) {
    announce_ally_to_ally_tribute(ctx, answer, what);
    return;
  }
  announce_reply_to_local_request(ctx, answer, key, what);
}

void announce_ally_to_ally_call(const Context& ctx,
                                const Game::Systems::AllyCallAnswer& answer) {
  const QString asker = ctx.name_of(answer.requester);
  const QString ally = ctx.name_of(answer.ally);
  const bool attack = answer.kind == Game::Systems::AllyCallKind::Attack;
  const bool accepted = answer.verdict == AllyCallVerdict::Accepted;
  if (attack) {
    ctx.sink.exchange(accepted
                          ? QCoreApplication::translate(
                                "GameEngine", "%1 called %2 to the attack; %2 marches.")
                                .arg(asker, ally)
                          : QCoreApplication::translate(
                                "GameEngine", "%1 called %2 to the attack; %2 stays.")
                                .arg(asker, ally),
                      accepted);
  } else {
    ctx.sink.exchange(
        accepted ? QCoreApplication::translate("GameEngine",
                                               "%1 called for help; %2 sends men.")
                       .arg(asker, ally)
                 : QCoreApplication::translate("GameEngine",
                                               "%1 called for help; %2 cannot come.")
                       .arg(asker, ally),
        accepted);
  }
}

void announce_local_call_outcome(const Context& ctx,
                                 std::vector<Game::Systems::AllyCallAnswer>& replies) {
  std::sort(replies.begin(), replies.end(), [](const auto& a, const auto& b) {
    return a.ally < b.ally;
  });
  const bool attack = replies.front().kind == Game::Systems::AllyCallKind::Attack;
  QStringList coming;
  const Game::Systems::AllyCallAnswer* speaker = nullptr;
  for (const auto& reply : replies) {
    if (reply.verdict == AllyCallVerdict::Accepted) {
      coming.append(ctx.name_of(reply.ally));
      if (speaker == nullptr || speaker->verdict != AllyCallVerdict::Accepted) {
        speaker = &reply;
      }
    } else if (speaker == nullptr) {
      speaker = &reply;
    }
  }
  if (coming.isEmpty()) {
    ctx.sink.exchange(attack ? QCoreApplication::translate(
                                   "GameEngine", "No ally will join the attack.")
                             : QCoreApplication::translate(
                                   "GameEngine", "No ally can spare men to defend it."),
                      false);
  } else {
    const QString names = QLocale().createSeparatedList(coming);
    ctx.sink.exchange(attack ? QCoreApplication::translate(
                                   "GameEngine", "%1 will march on that position.")
                                   .arg(names)
                             : QCoreApplication::translate(
                                   "GameEngine", "%1 will send men to hold it.")
                                   .arg(names),
                      true);
  }
  if (speaker == nullptr) {
    return;
  }
  CommanderMessageFact fact{
      .trigger = speaker->verdict == AllyCallVerdict::Accepted
                     ? CommanderMessageTrigger::CallAccepted
                     : CommanderMessageTrigger::CallRefused,
      .subject_owner_id = ctx.local,
      .actor_owner_id = speaker->ally,
      .subject_type = QLatin1String(Game::Systems::ally_call_kind_key(speaker->kind))};
  switch (speaker->verdict) {
  case AllyCallVerdict::RefusedUnderThreat:
    fact.reason = QStringLiteral("under_threat");
    break;
  case AllyCallVerdict::RefusedNoArmy:
    fact.reason = QStringLiteral("no_army");
    break;
  case AllyCallVerdict::RefusedUnwilling:
    fact.reason = QStringLiteral("unwilling");
    break;
  case AllyCallVerdict::Accepted:
    break;
  }
  ctx.sink.commander_fact(fact);
}

auto build_appeal_card(const Context& ctx,
                       const Game::Systems::AllyAppeal& appeal) -> QVariantMap {
  const QString name = ctx.name_of(appeal.from_ally);
  const QString key = QLatin1String(Game::Systems::resource_type_key(appeal.resource));
  QVariantMap card;
  card["id"] = appeal.appeal_id;
  card["from"] = appeal.from_ally;
  card["name"] = name;
  card["kind"] = QLatin1String(Game::Systems::ally_appeal_kind_key(appeal.kind));
  card["x"] = appeal.target_x;
  card["z"] = appeal.target_z;
  card["seconds"] = Game::Systems::k_ally_appeal_answer_seconds;
  switch (appeal.kind) {
  case AllyAppealKind::Resources:
    card["resource"] = key;
    card["amount"] = appeal.amount;
    card["text"] = QCoreApplication::translate("GameEngine", "%1 asks you for %2 %3.")
                       .arg(name)
                       .arg(appeal.amount)
                       .arg(ally_resource_word(key));
    card["accept"] =
        QCoreApplication::translate("GameEngine", "Give %1").arg(appeal.amount);
    card["decline"] = QCoreApplication::translate("GameEngine", "Refuse");
    ctx.sink.commander_fact({.trigger = CommanderMessageTrigger::AllyNeedsResources,
                             .subject_owner_id = ctx.local,
                             .actor_owner_id = appeal.from_ally,
                             .amount = appeal.amount,
                             .resource = key});
    break;
  case AllyAppealKind::Defend:
    card["text"] =
        QCoreApplication::translate(
            "GameEngine", "%1's camp is under attack. Will you send men to hold it?")
            .arg(name);
    card["accept"] = QCoreApplication::translate("GameEngine", "Send men");
    card["decline"] = QCoreApplication::translate("GameEngine", "Refuse");
    break;
  case AllyAppealKind::Attack:
    card["text"] =
        QCoreApplication::translate(
            "GameEngine", "%1 is marching on the enemy. Will you join the attack?")
            .arg(name);
    card["accept"] = QCoreApplication::translate("GameEngine", "Join");
    card["decline"] = QCoreApplication::translate("GameEngine", "Refuse");
    break;
  }
  return card;
}

void announce_appeal_follow_up(const Context& ctx,
                               const Game::Systems::AllyAppealReply& reply) {
  const QString name = ctx.name_of(reply.from_ally);
  const bool defend = reply.kind == AllyAppealKind::Defend;
  const QString key = QLatin1String(Game::Systems::resource_type_key(reply.resource));
  switch (reply.follow_up) {
  case AllyAppealFollowUp::Grateful:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine", "%1 thanks you for the %2 %3.")
            .arg(name)
            .arg(reply.amount)
            .arg(ally_resource_word(key)),
        true);
    break;
  case AllyAppealFollowUp::AwaitingAid:
    ctx.sink.exchange(
        defend ? QCoreApplication::translate("GameEngine",
                                             "%1 will hold until your men arrive.")
                     .arg(name)
               : QCoreApplication::translate("GameEngine",
                                             "%1 expects your men at the enemy's gate.")
                     .arg(name),
        true);
    break;
  case AllyAppealFollowUp::HoldAlone:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine", "%1 will hold the camp alone.")
            .arg(name),
        false);
    break;
  case AllyAppealFollowUp::MarchAlone:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine", "%1 marches alone.").arg(name),
        false);
    break;
  case AllyAppealFollowUp::ManageWithout:
    ctx.sink.exchange(
        QCoreApplication::translate("GameEngine", "%1 will manage without your help.")
            .arg(name),
        false);
    break;
  case AllyAppealFollowUp::AidArrived:
    ctx.sink.exchange(
        defend ? QCoreApplication::translate(
                     "GameEngine", "%1: your men have reached the camp. Well met.")
                     .arg(name)
               : QCoreApplication::translate("GameEngine",
                                             "%1: your men have joined the attack.")
                     .arg(name),
        true);
    break;
  case AllyAppealFollowUp::AidNeverCame:
    ctx.sink.exchange(
        QCoreApplication::translate(
            "GameEngine",
            "%1: the men you promised never came. That will be remembered.")
            .arg(name),
        false);
    break;
  case AllyAppealFollowUp::Withdrawn:
    ctx.sink.exchange(QCoreApplication::translate(
                          "GameEngine", "%1 heard no answer and withdraws the request.")
                          .arg(name),
                      false);
    break;
  }
}

} // namespace

auto owner_display_name(const Game::Session::SessionContext* session,
                        int owner_id) -> QString {
  if (session != nullptr) {
    for (const auto& owner : session->owners().get_all_owners()) {
      if (owner.owner_id == owner_id) {
        return QString::fromStdString(owner.name);
      }
    }
  }
  return QCoreApplication::translate("GameEngine", "your ally");
}

auto is_friendly_commander(const Game::Session::SessionContext* session,
                           int local_owner_id,
                           int owner_id) -> bool {
  return session != nullptr && owner_id != local_owner_id &&
         session->owners().is_ai(owner_id) &&
         session->owners().are_allies(local_owner_id, owner_id);
}

auto ally_resource_word(const QString& resource_key) -> QString {
  Game::Systems::ResourceType type{};
  if (!Game::Systems::resource_type_from_key(resource_key, type)) {
    return resource_key;
  }
  switch (type) {
  case Game::Systems::ResourceType::Gold:
    return QCoreApplication::translate("GameEngine", "gold");
  case Game::Systems::ResourceType::Food:
    return QCoreApplication::translate("GameEngine", "food");
  case Game::Systems::ResourceType::Wood:
    return QCoreApplication::translate("GameEngine", "wood");
  case Game::Systems::ResourceType::Stone:
    return QCoreApplication::translate("GameEngine", "stone");
  case Game::Systems::ResourceType::Iron:
    return QCoreApplication::translate("GameEngine", "iron");
  default:
    break;
  }
  return resource_key;
}

void AllyAnnouncementPresenter::announce_all(Game::Session::SessionContext* session,
                                             int local_owner_id) const {
  announce_exchanges(session, local_owner_id);
  announce_calls(session, local_owner_id);
  announce_appeals(session, local_owner_id);
}

void AllyAnnouncementPresenter::announce_exchanges(
    Game::Session::SessionContext* session, int local_owner_id) const {
  if (session == nullptr) {
    return;
  }
  const auto answers = session->marketplace().take_ally_answers();
  if (answers.empty()) {
    return;
  }
  const Context ctx{*session, local_owner_id, m_sink};
  for (const auto& answer : answers) {
    announce_tribute_answer(ctx, answer);
  }
}

void AllyAnnouncementPresenter::announce_calls(Game::Session::SessionContext* session,
                                               int local_owner_id) const {
  if (session == nullptr) {
    return;
  }
  const auto answers = session->alliance().take_call_answers();
  if (answers.empty()) {
    return;
  }
  const Context ctx{*session, local_owner_id, m_sink};
  std::map<std::uint32_t, std::vector<Game::Systems::AllyCallAnswer>> by_call;
  for (const auto& answer : answers) {
    if (answer.requester == ctx.local) {
      by_call[answer.call_id].push_back(answer);
    } else if (ctx.friendly(answer.requester) && ctx.friendly(answer.ally)) {
      announce_ally_to_ally_call(ctx, answer);
    }
  }
  for (auto& [call_id, replies] : by_call) {
    (void)call_id;
    announce_local_call_outcome(ctx, replies);
  }
}

void AllyAnnouncementPresenter::announce_appeals(Game::Session::SessionContext* session,
                                                 int local_owner_id) const {
  if (session == nullptr) {
    return;
  }
  const Context ctx{*session, local_owner_id, m_sink};
  auto& board = session->alliance();
  for (const auto& appeal : board.take_new_appeals()) {
    if (appeal.to_owner != ctx.local) {
      continue;
    }
    m_sink.appeal_opened(build_appeal_card(ctx, appeal));
  }

  for (const auto& reply : board.take_appeal_replies()) {
    if (reply.to_owner != ctx.local) {
      continue;
    }
    if (reply.follow_up != AllyAppealFollowUp::AidArrived &&
        reply.follow_up != AllyAppealFollowUp::AidNeverCame) {
      m_sink.appeal_closed(reply.appeal_id);
    }
    announce_appeal_follow_up(ctx, reply);
  }
}

} // namespace App::World
