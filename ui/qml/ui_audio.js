.pragma library

function play_cue(audio, cue_id) {
    if (!audio || !audio.play_cue)
        return;
    audio.play_cue(cue_id);
}

function play_hover(audio) {
    play_cue(audio, "ui.hover");
}

function play_click(audio) {
    play_cue(audio, "ui.click");
}

function play_back(audio) {
    play_cue(audio, "ui.back");
}

function play_tab_switch(audio) {
    play_cue(audio, "ui.tab_switch");
}

function play_panel_open(audio) {
    play_cue(audio, "ui.panel_open");
}

function play_panel_close(audio) {
    play_cue(audio, "ui.panel_close");
}

function play_toggle(audio) {
    play_cue(audio, "ui.toggle");
}

function play_error(audio) {
    play_cue(audio, "ui.error");
}
