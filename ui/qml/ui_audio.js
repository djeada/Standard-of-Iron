.pragma library

function play_hover(audio) {
    if (!audio)
        return;
    audio.play_ui_hover();
}

function play_click(audio) {
    if (!audio)
        return;
    audio.play_ui_click();
}
