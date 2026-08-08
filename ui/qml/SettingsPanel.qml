import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    signal cancelled
    property bool syncing_audio_sliders: false

    function color_vision_label(mode) {
        switch (mode) {
        case "protanopia":
            return qsTr("Protanopia (red-blind)");
        case "deuteranopia":
            return qsTr("Deuteranopia (green-blind)");
        case "tritanopia":
            return qsTr("Tritanopia (blue-blind)");
        default:
            return qsTr("Standard");
        }
    }

    function set_audio_slider_values() {
        master_volume_slider.value = game.audio_system.get_master_volume() * 100;
        music_volume_slider.value = game.audio_system.get_music_volume() * 100;
        sfx_volume_slider.value = game.audio_system.get_sound_volume() * 100;
        voice_volume_slider.value = game.audio_system.get_voice_volume() * 100;
        ambience_volume_slider.value = game.audio_system.get_ambience_volume() * 100;
    }

    function set_audio_volume(volume_name, value) {
        if (syncing_audio_sliders || typeof game === 'undefined' || !game.audio_system)
            return;
        var normalized = Math.max(0, Math.min(1, value / 100));
        if (volume_name === "master")
            game.audio_system.set_master_volume(normalized);
        else if (volume_name === "music")
            game.audio_system.set_music_volume(normalized);
        else if (volume_name === "sfx")
            game.audio_system.set_sound_volume(normalized);
        else if (volume_name === "voice")
            game.audio_system.set_voice_volume(normalized);
        else if (volume_name === "ambience")
            game.audio_system.set_ambience_volume(normalized);
    }

    function sync_audio_sliders() {
        if (typeof game === 'undefined' || !game.audio_system)
            return;
        syncing_audio_sliders = true;
        set_audio_slider_values();
        syncing_audio_sliders = false;
    }

    anchors.fill: parent
    z: 25
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        forceActiveFocus();
        sync_audio_sliders();
    }
    onVisibleChanged: {
        if (visible)
            sync_audio_sliders();
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.9, Design.A11y.scaled(700))
        height: Math.min(parent.height * 0.9, Design.A11y.scaled(600))
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: Theme.panelBr
        border.width: 1
        opacity: 0.98

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingXLarge
            spacing: Theme.spacingLarge

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: qsTr("Settings")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: qsTr("Close")
                    button_style: "secondary"
                    onClicked: root.cancelled()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    width: container.width - Theme.spacingXLarge * 2
                    spacing: Theme.spacingLarge

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Audio Settings")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingMedium
                            columnSpacing: Theme.spacingMedium

                            Label {
                                text: qsTr("Master Volume:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Design.IronSlider {
                                    id: master_volume_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: 100
                                    stepSize: 1
                                    onValueChanged: {
                                        root.set_audio_volume("master", value);
                                    }
                                }

                                Label {
                                    text: Design.Numerals.percent(master_volume_slider.value)
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 88
                                }
                            }

                            Label {
                                text: qsTr("Music Volume:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Design.IronSlider {
                                    id: music_volume_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: 100
                                    stepSize: 1
                                    onValueChanged: {
                                        root.set_audio_volume("music", value);
                                    }
                                }

                                Label {
                                    text: Design.Numerals.percent(music_volume_slider.value)
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 88
                                }
                            }

                            Label {
                                text: qsTr("SFX Volume:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Design.IronSlider {
                                    id: sfx_volume_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: 100
                                    stepSize: 1
                                    onValueChanged: {
                                        root.set_audio_volume("sfx", value);
                                    }
                                }

                                Label {
                                    text: Design.Numerals.percent(sfx_volume_slider.value)
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 88
                                }
                            }

                            Label {
                                text: qsTr("Voice Volume:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Design.IronSlider {
                                    id: voice_volume_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: 100
                                    stepSize: 1
                                    onValueChanged: {
                                        root.set_audio_volume("voice", value);
                                    }
                                }

                                Label {
                                    text: Design.Numerals.percent(voice_volume_slider.value)
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 88
                                }
                            }

                            Label {
                                text: qsTr("Ambience Volume:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Design.IronSlider {
                                    id: ambience_volume_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: 100
                                    stepSize: 1
                                    onValueChanged: {
                                        root.set_audio_volume("ambience", value);
                                    }
                                }

                                Label {
                                    text: Design.Numerals.percent(ambience_volume_slider.value)
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 88
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Graphics Settings")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingMedium
                            columnSpacing: Theme.spacingMedium

                            Label {
                                text: qsTr("Graphics Quality:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            StyledComboBox {
                                id: graphics_quality_combo_box

                                Layout.fillWidth: true
                                model: typeof graphics_settings !== 'undefined' ? graphics_settings.quality_options : ["Low", "Medium", "High", "Ultra"]
                                currentIndex: typeof graphics_settings !== 'undefined' ? graphics_settings.quality_level : 1
                                onActivated: function (index) {
                                    if (typeof graphics_settings !== 'undefined')
                                        graphics_settings.quality_level = index;
                                }
                            }

                            Label {
                                text: typeof graphics_settings !== 'undefined' ? graphics_settings.get_quality_description() : ""
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                opacity: 0.7
                                wrapMode: Text.WordWrap
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Controls")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingMedium
                            columnSpacing: Theme.spacingMedium

                            Design.IronCheckBox {
                                Layout.columnSpan: 2
                                text: qsTr("Edge scrolling")
                                description: qsTr("Pans the camera when the cursor reaches the edge of the screen")
                                checked: UiPreferences.edgeScrollEnabled
                                onToggled: UiPreferences.edgeScrollEnabled = checked
                            }

                            Label {
                                text: qsTr("Edge scroll speed:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                                enabled: UiPreferences.edgeScrollEnabled
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium

                                Design.IronSlider {
                                    id: edge_scroll_slider

                                    blocked: !UiPreferences.edgeScrollEnabled
                                    Layout.fillWidth: true
                                    from: UiPreferences.minEdgeScrollSensitivity
                                    to: UiPreferences.maxEdgeScrollSensitivity
                                    stepSize: 0.05
                                    snapMode: Slider.SnapAlways
                                    value: UiPreferences.edgeScrollSensitivity
                                    onMoved: UiPreferences.edgeScrollSensitivity = value
                                }

                                Label {
                                    text: Design.Numerals.percent(edge_scroll_slider.value * 100)
                                    color: Theme.textMain
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.preferredWidth: 96
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }

                        ControlsBindingList {
                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Autosave")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingMedium
                            columnSpacing: Theme.spacingMedium

                            Label {
                                text: qsTr("Autosaves to keep:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium

                                Design.IronSlider {
                                    id: autosave_slot_slider

                                    Layout.fillWidth: true
                                    from: 1
                                    to: 10
                                    stepSize: 1
                                    snapMode: Slider.SnapAlways
                                    value: typeof game !== 'undefined' ? game.saves.autosave_slot_count : 3
                                    onMoved: {
                                        if (typeof game !== 'undefined')
                                            game.saves.autosave_slot_count = Math.round(value);
                                    }
                                }

                                Label {
                                    text: Design.Numerals.roman(autosave_slot_slider.value)
                                    color: Theme.textMain
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.preferredWidth: 56
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label {
                                text: qsTr("Autosave every:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium

                                Design.IronSlider {
                                    id: autosave_interval_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 60
                                    stepSize: 5
                                    snapMode: Slider.SnapAlways
                                    value: typeof game !== 'undefined' ? game.saves.autosave_interval_minutes : 5
                                    onMoved: {
                                        if (typeof game !== 'undefined')
                                            game.saves.autosave_interval_minutes = Math.round(value);
                                    }
                                }

                                Label {
                                    text: autosave_interval_slider.value <= 0 ? qsTr("Off") : qsTr("%1 min").arg(Design.Numerals.roman(autosave_interval_slider.value))
                                    color: Theme.textMain
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.preferredWidth: 104
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label {
                                text: qsTr("Older autosaves beyond this count are deleted automatically.")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                opacity: 0.7
                                wrapMode: Text.WordWrap
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Accessibility")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingMedium
                            columnSpacing: Theme.spacingMedium

                            Label {
                                text: qsTr("Interface Scale:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium

                                Design.IronSlider {
                                    id: ui_scale_slider

                                    Layout.fillWidth: true
                                    from: UiPreferences.minUiScale
                                    to: UiPreferences.maxUiScale
                                    stepSize: 0.05
                                    snapMode: Slider.SnapAlways
                                    value: UiPreferences.uiScale
                                    onMoved: UiPreferences.uiScale = value
                                }

                                Label {
                                    text: Design.Numerals.percent(ui_scale_slider.value * 100)
                                    color: Theme.textMain
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.preferredWidth: 96
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label {
                                text: qsTr("Colour Vision:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            StyledComboBox {
                                id: color_vision_combo_box

                                Layout.fillWidth: true
                                model: UiPreferences.colorVisionModes
                                currentIndex: Math.max(0, UiPreferences.colorVisionModes.indexOf(UiPreferences.colorVisionMode))
                                displayText: root.color_vision_label(currentText)
                                onActivated: function (index) {
                                    UiPreferences.colorVisionMode = UiPreferences.colorVisionModes[index];
                                }
                                delegate_text: function (data) {
                                    return root.color_vision_label(data);
                                }
                            }

                            Design.IronCheckBox {
                                Layout.columnSpan: 2
                                text: qsTr("Reduce motion")
                                description: qsTr("Removes transitions and idle animations across every screen")
                                checked: UiPreferences.reducedMotion
                                onToggled: UiPreferences.reducedMotion = checked
                            }

                            Design.IronCheckBox {
                                Layout.columnSpan: 2
                                text: qsTr("High contrast")
                                description: qsTr("Raises panel and text contrast for low vision")
                                checked: UiPreferences.highContrast
                                onToggled: UiPreferences.highContrast = checked
                            }

                            Design.IronCheckBox {
                                Layout.columnSpan: 2
                                text: qsTr("Always show keyboard focus")
                                description: qsTr("Keeps the focus outline visible even after clicking")
                                checked: UiPreferences.alwaysShowFocus
                                onToggled: UiPreferences.alwaysShowFocus = checked
                            }

                            Design.IronCheckBox {
                                Layout.columnSpan: 2
                                text: qsTr("Team ring patterns")
                                description: UiPreferences.colorVisionMode !== "none" ? qsTr("On automatically while a colour vision mode is selected") : qsTr("Marks each side with its own selection ring shape as well as its colour")
                                checked: UiPreferences.effectiveTeamPatterns
                                blocked: UiPreferences.colorVisionMode !== "none"
                                onToggled: UiPreferences.teamPatterns = checked
                            }

                            Design.IronCheckBox {
                                Layout.columnSpan: 2
                                text: qsTr("Damage numbers")
                                description: qsTr("Shows the damage dealt above each hit while leading the commander")
                                checked: UiPreferences.damageNumbers
                                onToggled: UiPreferences.damageNumbers = checked
                            }

                            Label {
                                text: qsTr("Screen effects:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium

                                Design.IronSlider {
                                    id: screen_effect_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: 0.05
                                    snapMode: Slider.SnapAlways
                                    value: UiPreferences.screenEffectIntensity
                                    onMoved: UiPreferences.screenEffectIntensity = value
                                }

                                Label {
                                    text: screen_effect_slider.value <= 0 ? qsTr("Off") : Design.Numerals.percent(screen_effect_slider.value * 100)
                                    color: Theme.textMain
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.preferredWidth: 96
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label {
                                text: qsTr("Camera motion:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium

                                Design.IronSlider {
                                    id: camera_motion_slider

                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: 0.05
                                    snapMode: Slider.SnapAlways
                                    value: UiPreferences.cameraMotionScale
                                    onMoved: UiPreferences.cameraMotionScale = value
                                }

                                Label {
                                    text: camera_motion_slider.value <= 0 ? qsTr("Off") : Design.Numerals.percent(camera_motion_slider.value * 100)
                                    color: Theme.textMain
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.preferredWidth: 96
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label {
                                text: qsTr("Reduces head bob and sway while leading the commander. It never limits camera movement you ask for.")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                opacity: 0.7
                                wrapMode: Text.WordWrap
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                            }

                            Label {
                                text: qsTr("These settings apply to the campaign, skirmish and editor tools alike.")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                opacity: 0.7
                                wrapMode: Text.WordWrap
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Language")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingMedium
                            columnSpacing: Theme.spacingMedium

                            Label {
                                text: qsTr("Select Language:")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeMedium
                            }

                            StyledComboBox {
                                id: language_combo_box

                                Layout.fillWidth: true
                                model: typeof language_manager !== 'undefined' ? language_manager.available_languages : []
                                currentIndex: {
                                    if (typeof language_manager === 'undefined')
                                        return 0;
                                    var idx = language_manager.available_languages.indexOf(language_manager.current_language);
                                    return idx >= 0 ? idx : 0;
                                }
                                displayText: {
                                    if (typeof language_manager === 'undefined' || !currentText)
                                        return "";
                                    return language_manager.language_display_name(currentText);
                                }
                                onActivated: function (index) {
                                    if (typeof language_manager !== 'undefined' && currentText)
                                        language_manager.set_language(currentText);
                                }
                                delegate_text: function (data) {
                                    return typeof language_manager !== 'undefined' ? language_manager.language_display_name(data) : data;
                                }
                            }

                            Label {
                                text: qsTr("Language changes apply immediately")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                opacity: 0.7
                                Layout.columnSpan: 2
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("About")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 2
                            color: Theme.border
                            opacity: 0.5
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSmall

                            Label {
                                text: qsTr("Standard of Iron - RTS Game")
                                color: Theme.textMain
                                font.pointSize: Theme.fontSizeMedium
                                font.bold: true
                            }

                            Label {
                                text: qsTr("Version %1").arg(Qt.application.version)
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: Theme.border
                                opacity: 0.3
                                Layout.topMargin: Theme.spacingSmall
                                Layout.bottomMargin: Theme.spacingSmall
                            }

                            Label {
                                text: qsTr("Third-Party Software")
                                color: Theme.textMain
                                font.pointSize: Theme.fontSizeMedium
                                font.bold: true
                            }

                            Label {
                                text: qsTr("This game uses the Qt framework, licensed under the GNU Lesser General Public License v3 (LGPL v3).")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Label {
                                text: qsTr("Qt is dynamically linked, allowing you to replace Qt libraries with your own versions.")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Label {
                                text: "<a href='https://www.gnu.org/licenses/lgpl-3.0.html'>" + qsTr("LGPL v3 License") + "</a> | <a href='https://www.qt.io'>" + qsTr("Qt Website") + "</a>"
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                textFormat: Text.RichText
                                onLinkActivated: function (link) {
                                    Qt.openUrlExternally(link);
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.NoButton
                                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                }
                            }

                            Label {
                                text: qsTr("Music generated with Meta's AudioCraft, whose models are licensed CC BY-NC 4.0. This game is distributed free of charge, which that licence permits; it may not be sold.")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Label {
                                text: qsTr("Voice lines recorded by Adam Djellouli.")
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Label {
                                text: "<a href='https://creativecommons.org/licenses/by-nc/4.0/'>" + qsTr("CC BY-NC 4.0 License") + "</a> | <a href='https://github.com/facebookresearch/audiocraft'>" + qsTr("AudioCraft") + "</a>"
                                color: Theme.textSub
                                font.pointSize: Theme.fontSizeSmall
                                textFormat: Text.RichText
                                onLinkActivated: function (link) {
                                    Qt.openUrlExternally(link);
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.NoButton
                                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
