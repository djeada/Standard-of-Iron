import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0

Item {
    id: root

    signal cancelled
    property bool syncing_audio_sliders: false

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

        width: Math.min(parent.width * 0.6, 700)
        height: Math.min(parent.height * 0.8, 600)
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

                                Slider {
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
                                    text: Math.round(master_volume_slider.value) + "%"
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 45
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

                                Slider {
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
                                    text: Math.round(music_volume_slider.value) + "%"
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 45
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

                                Slider {
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
                                    text: Math.round(sfx_volume_slider.value) + "%"
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 45
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

                                Slider {
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
                                    text: Math.round(voice_volume_slider.value) + "%"
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 45
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

                                Slider {
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
                                    text: Math.round(ambience_volume_slider.value) + "%"
                                    color: Theme.textSub
                                    font.pointSize: Theme.fontSizeMedium
                                    Layout.minimumWidth: 45
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
                                text: qsTr("Version 1.0.0")
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
                                text: "<a href='https://www.gnu.org/licenses/lgpl-3.0.html'>LGPL v3 License</a> | <a href='https://www.qt.io'>Qt Website</a>"
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
