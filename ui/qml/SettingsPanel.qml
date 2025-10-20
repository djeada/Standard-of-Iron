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

            // Language Settings Section
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

                        delegate: ItemDelegate {
                            width: languageComboBox.width
                            contentItem: Text {
                                text: typeof languageManager !== 'undefined' ? languageManager.languageDisplayName(modelData) : modelData
                                color: Theme.textMain
                                font.pointSize: Theme.fontSizeMedium
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: languageComboBox.highlightedIndex === index
                        }

                        onCurrentTextChanged: {
                            if (typeof languageManager !== 'undefined' && currentText) {
                                languageManager.setLanguage(currentText);
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

            Item {
                Layout.fillHeight: true
            }

            // Info text
            Label {
                Layout.fillWidth: true
                text: qsTr("More settings coming soon...")
                color: Theme.textSub
                font.pointSize: Theme.fontSizeSmall
                opacity: 0.6
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
