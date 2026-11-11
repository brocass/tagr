import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: row
    objectName: "trackRow"
    required property int index
    required property string path
    required property string fileName
    required property string title
    required property string artist
    required property string albumArtist
    required property string album
    required property string genre
    required property int trackNumber
    required property string durationText
    required property string format
    required property bool selected
    required property bool dirty
    property real fileColumnWidth: 220
    property bool showPath: true
    property bool showTitle: true
    property bool showArtist: true
    property bool showAlbumArtist: true
    property bool showAlbum: true
    property bool showGenre: false
    property bool showTrack: true
    property bool showDuration: true
    property bool showFormat: true
    property bool editing: false
    property string editText: row.fileName

    signal renameRequested(int index)
    signal renameFinished(int index)

    function beginRename() {
        row.editText = row.fileName
        row.renameRequested(row.index)
    }

    // Commit the typed name and leave edit mode.
    function commitRename() {
        if (!row.editing)
            return
        const newName = fileNameField.text.trimmed()
        if (newName.length > 0 && newName !== row.fileName)
            backend.renameTrackFile(row.path, newName)
        row.renameFinished(row.index)
    }

    // Leave edit mode without touching the file.
    function cancelRename() {
        if (!row.editing)
            return
        fileNameField.text = row.fileName
        row.renameFinished(row.index)
    }

    onEditingChanged: {
        if (!editing)
            return
        Qt.callLater(function() {
            fileNameField.forceActiveFocus()
            // Select the name but keep the extension, so typing replaces only the name.
            const dot = row.fileName.lastIndexOf(".")
            fileNameField.select(0, dot > 0 ? dot : fileNameField.length)
        })
    }

    // Selected rows sit on the accent color, so every label needs a foreground
    // that reads on it instead of the fixed dark-theme grays.
    readonly property color primaryText: row.selected ? theme.accentForeground : "#c7c7cf"
    readonly property color secondaryText: row.selected ? Qt.alpha(theme.accentForeground, 0.82) : "#92929c"

    signal rowClicked(int index, bool control, bool shift)
    signal rowDoubleClicked(int index)

    width: ListView.view ? ListView.view.width : 0
    height: 22

    Rectangle {
        anchors.fill: parent
        color: row.selected ? theme.accent : mouse.containsMouse ? "#1a1a1f" : row.index % 2 ? "#101013" : "#121216"
        Rectangle { width: 2; height: parent.height; color: theme.accentForeground; visible: row.selected }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 0

        Item {
            width: row.fileColumnWidth
            height: parent.height
            Rectangle {
                x: 3
                width: 9
                height: 9
                anchors.verticalCenter: parent.verticalCenter
                color: row.selected ? theme.accentForeground : "transparent"
                border.color: row.selected ? theme.accentForeground : "#55555f"
                border.width: 1
            }
            Rectangle {
                x: 17
                width: 4
                height: 4
                radius: 2
                anchors.verticalCenter: parent.verticalCenter
                color: row.selected ? theme.accentForeground : theme.accent
                visible: row.dirty
            }
            Label {
                id: fileNameLabel
                visible: !row.editing
                anchors.left: parent.left
                anchors.leftMargin: 25
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                text: row.fileName
                color: row.selected ? theme.accentForeground : "#d5d5dc"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
            TextField {
                id: fileNameField
                visible: row.editing
                anchors.left: parent.left
                anchors.leftMargin: 25
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                height: 22
                text: row.editText
                font.pixelSize: 11
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                selectByMouse: true
                color: "#ededf1"
                selectionColor: Qt.darker(theme.accent, 1.7)
                selectedTextColor: "#ffffff"
                placeholderTextColor: "#92929c"
                background: Rectangle {
                    color: "#17171c"
                    border.color: fileNameField.activeFocus ? theme.accent : "#33333b"
                    border.width: 1
                    radius: Math.min(2, theme.radius)
                }
                onAccepted: row.commitRename()
                onEditingFinished: row.commitRename()
                Keys.onEscapePressed: row.cancelRename()
            }
        }
        Label {
            visible: row.showPath
            width: visible ? 220 : 0
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            leftPadding: 3
            rightPadding: 5
            text: row.path.slice(0, Math.max(0, row.path.length - row.fileName.length - 1))
            color: row.secondaryText
            font.pixelSize: 10
            elide: Text.ElideMiddle
        }
        Label { visible: row.showTitle; width: visible ? 160 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; rightPadding: 5; text: row.title; color: row.primaryText; font.pixelSize: 11; elide: Text.ElideRight }
        Label { visible: row.showArtist; width: visible ? 130 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; rightPadding: 5; text: row.artist; color: row.primaryText; font.pixelSize: 11; elide: Text.ElideRight }
        Label { visible: row.showAlbumArtist; width: visible ? 140 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; rightPadding: 5; text: row.albumArtist; color: row.secondaryText; font.pixelSize: 11; elide: Text.ElideRight }
        Label { visible: row.showAlbum; width: visible ? 140 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; rightPadding: 5; text: row.album; color: row.primaryText; font.pixelSize: 11; elide: Text.ElideRight }
        Label { visible: row.showGenre; width: visible ? 120 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; rightPadding: 5; text: row.genre; color: row.primaryText; font.pixelSize: 11; elide: Text.ElideRight }
        Label {
            visible: row.showTrack
            width: visible ? 42 : 0
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignRight
            rightPadding: 6
            text: row.trackNumber > 0 ? row.trackNumber.toLocaleString(Qt.locale(), "f", 0) : ""
            color: row.secondaryText
            font.pixelSize: 10
        }
        Label { visible: row.showDuration; width: visible ? 50 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; text: row.durationText; color: row.secondaryText; font.pixelSize: 10 }
        Label { visible: row.showFormat; width: visible ? 46 : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 3; text: row.format; color: row.secondaryText; font.pixelSize: 10; elide: Text.ElideRight }
    }

    Menu {
        id: rowMenu
        MenuItem {
            text: qsTr("Play")
            icon.name: "media-playback-start"
            onTriggered: backend.playPath(row.path)
        }
        MenuItem {
            text: qsTr("Rename")
            onTriggered: {
                row.beginRename()
                rowMenu.close()
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        // While renaming, let clicks reach the text field instead of the row.
        enabled: !row.editing
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function(event) {
            if (event.button === Qt.RightButton) {
                row.rowClicked(row.index, false, false);
                rowMenu.popup(event.position.x, event.position.y);
                event.accepted = true;
                return;
            }
            row.rowClicked(row.index, event.modifiers & Qt.ControlModifier, event.modifiers & Qt.ShiftModifier);
        }
        onDoubleClicked: function(event) {
            if (event.button === Qt.LeftButton) {
                row.rowClicked(row.index, false, false);
                row.beginRename()
                event.accepted = true;
            }
        }
    }
}
