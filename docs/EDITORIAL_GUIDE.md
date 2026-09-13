# Documentation Editorial Guide

The documentation in this repository serves two audiences at once: contributors who need precise implementation details and readers who may encounter a page as a standalone article. A good document should work for both.

The goal is not to make technical writing less technical. It is to make the technical content easier to enter, follow, and reuse without losing accuracy.

## The article-ready standard

A document is article-ready when a reader can open it without repository context and quickly understand:

1. **what the subject is;**
2. **why it exists or matters;**
3. **how the important pieces fit together;** and
4. **where to look for exact implementation details.**

The opening should establish those points before diving into class names, file paths, tuning values, or edge cases.

## Recommended structure

Use the following shape when it fits the subject. Not every page needs every section.

```text
# Descriptive title

Short introduction: what this is, why it exists, and what the reader will learn.

## Core concept or design goal

Explain the idea before the machinery.

## How it works

Describe the system in a logical sequence.

## Data, configuration, or API

Document exact fields, files, commands, and constraints.

## Operational guidance

Explain how to use, tune, debug, or extend the system.

## Invariants and failure modes

Call out rules that must remain true and the consequences of violating them.

## Related documentation

Link only to genuinely useful follow-up material.
```

Prefer descriptive section names over generic headings such as `Details`, `Notes`, or `Miscellaneous`.

## Write for a reader, not for the edit history

Documentation should describe the current system. Avoid prose that depends on knowing the sequence in which features were implemented.

Prefer:

> The renderer samples the fixed-tick replay at the requested presentation frame rate.

Instead of:

> We now sample the replay instead of rerunning the simulation like we used to.

Historical context belongs in a dedicated rationale section only when it helps explain a constraint that still matters.

## Lead with meaning, then names

Introduce the concept before listing implementation symbols.

Prefer:

> Formations maintain stable slot identities so units can reflow without losing their intended position.

Then explain the relevant classes, functions, IDs, and files.

A page that begins with a wall of identifiers forces readers to reverse-engineer the idea from the implementation.

## Keep paragraphs focused

A paragraph should normally develop one idea. Split it when it starts doing several jobs at once—for example, explaining behavior, naming a test, documenting an exception, and giving tuning advice in the same block.

Short paragraphs are not automatically better; coherent paragraphs are. Use lists when readers need to scan distinct items, not simply to avoid writing prose.

## Preserve precision

Editorial cleanup must not weaken technical claims.

Keep these details exact when they are part of the contract:

- class, function, field, test, and file names;
- command-line flags and example commands;
- units and numerical limits;
- ordering constraints;
- deterministic behavior;
- ownership and lifetime rules;
- schema requirements; and
- statements enforced by tests or CI.

If a value is expected to change through balancing or tuning, prefer linking to the authoritative data file instead of copying the value into prose.

## Separate intent from tuning

Durable documentation explains why a system is shaped the way it is. Frequently changing values should live in configuration, assets, or source code unless the value itself is part of a public contract.

This distinction keeps articles useful even when the game is rebalanced.

## Use headings as a narrative outline

A reader should be able to skim only the headings and still understand the path through the article. Headings should therefore describe the subject of the section, not the act of documenting it.

Prefer:

- `## How pressure arrives`
- `## Deterministic simulation`
- `## Failure and recovery`

Over:

- `## More details`
- `## Implementation notes`
- `## Other`

Use one H1 per document and keep heading levels hierarchical.

## Tables, lists, and code blocks

Use a table when readers need to compare the same attributes across several items. Use a list for independent facts, steps, or options. Use prose when the relationship between ideas matters more than scanning.

Fence commands and code with an appropriate language identifier whenever practical, such as `sh`, `cpp`, `json`, or `qml`.

Introduce every substantial code block with a sentence that explains what the reader should notice or do with it.

## Links and repository references

Use relative Markdown links for other repository documents. Put file paths, identifiers, flags, and literal values in backticks.

Do not add a long `Related documentation` section merely to collect links. Link a document at the point where it becomes useful, and add a final section only when readers genuinely need a small set of next steps.

## Tone

Use direct, calm, technical prose. Prefer concrete verbs and active voice where ownership is clear.

Avoid:

- filler such as “obviously,” “simply,” or “just”;
- promotional language inside engineering documentation;
- unexplained jokes or metaphors that obscure a rule;
- conversational fragments that depend on surrounding discussion; and
- repeated statements of the same conclusion.

A little personality is welcome when it makes the design easier to remember, but clarity wins whenever the two compete.

## Before merging a documentation change

Read the page once as if it were published outside GitHub. Confirm that:

- the opening establishes context without requiring another page;
- the section order follows the reader's likely questions;
- acronyms and project-specific terms are introduced before heavy use;
- long paragraphs contain one coherent idea;
- lists and tables are used for scanning rather than decoration;
- code, paths, and identifiers are formatted consistently;
- duplicated tuning values have been removed when an authoritative source exists;
- links still point to the right source of truth; and
- the ending leaves the reader with a clear understanding of the system rather than an abrupt collection of notes.

Treat this as an editorial standard, not a rigid template. Different subjects need different shapes, but every page should be understandable, technically trustworthy, and ready to stand on its own.
