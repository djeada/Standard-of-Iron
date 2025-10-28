import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron.UI 1.0

Item {
    id: root

    signal cancelled()

    anchors.fill: parent
    z: 25
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        forceActiveFocus();
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.6, 700)
        height: Math.min(parent.height * 0.7, 500)
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

                Button {
                    text: qsTr("Close")
                    onClicked: root.cancelled()
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
                            id: masterVolumeSlider

                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 100
                            stepSize: 1
                            onValueChanged: {
                                if (typeof gameEngine !== 'undefined' && gameEngine.audioSystem)
                                    gameEngine.audioSystem.setMasterVolume(value / 100);

                            }
                        }

                        Label {
                            text: Math.round(masterVolumeSlider.value) + "%"
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
                            id: musicVolumeSlider

                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 100
                            stepSize: 1
                            onValueChanged: {
                                if (typeof gameEngine !== 'undefined' && gameEngine.audioSystem)
                                    gameEngine.audioSystem.setMusicVolume(value / 100);

                            }
                        }

                        Label {
                            text: Math.round(musicVolumeSlider.value) + "%"
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
                            id: sfxVolumeSlider

                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 100
                            stepSize: 1
                            onValueChanged: {
                                if (typeof gameEngine !== 'undefined' && gameEngine.audioSystem)
                                    gameEngine.audioSystem.setSoundVolume(value / 100);

                            }
                        }

                        Label {
                            text: Math.round(sfxVolumeSlider.value) + "%"
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
                            id: voiceVolumeSlider

                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: 100
                            stepSize: 1
                            onValueChanged: {
                                if (typeof gameEngine !== 'undefined' && gameEngine.audioSystem)
                                    gameEngine.audioSystem.setVoiceVolume(value / 100);

                            }
                        }

                        Label {
                            text: Math.round(voiceVolumeSlider.value) + "%"
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

                    ComboBox {
                        id: languageComboBox

                        Layout.fillWidth: true
                        model: typeof languageManager !== 'undefined' ? languageManager.availableLanguages : []
                        currentIndex: {
                            if (typeof languageManager === 'undefined')
                                return 0;

                            var idx = languageManager.availableLanguages.indexOf(languageManager.currentLanguage);
                            return idx >= 0 ? idx : 0;
                        }
                        displayText: {
                            if (typeof languageManager === 'undefined' || !currentText)
                                return "";

                            return languageManager.languageDisplayName(currentText);
                        }
                        onCurrentTextChanged: {
                            if (typeof languageManager !== 'undefined' && currentText)
                                languageManager.setLanguage(currentText);

                        }

                        delegate: ItemDelegate {
                            width: languageComboBox.width
                            highlighted: languageComboBox.highlightedIndex === index

                            contentItem: Text {
                                text: typeof languageManager !== 'undefined' ? languageManager.languageDisplayName(modelData) : modelData
                                color: Theme.textMain
                                font.pointSize: Theme.fontSizeMedium
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

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
                        onLinkActivated: function(link) {
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

            Item {
                Layout.fillHeight: true
            }

        }

    }

}
