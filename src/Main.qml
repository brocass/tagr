import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    minimumWidth: 480
    minimumHeight: 480
    visible: true
    title: qsTr("Tagr — Audio tag editor")
    color: "#0e0e11"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool columnSettingsVisible: false
    property string pendingAction: ""
    property string pendingPath: ""
    property bool allowClose: false
    property bool editingText: search.activeFocus || tagEditor.activeFocus

    component CompactMenuItem: MenuItem {
        implicitHeight: 25
        font.pixelSize: 11
        leftPadding: 10
        rightPadding: 14
    }

    component HeaderItem: Item {
        property string caption
        property string column
        property bool alignRight: false
        height: 24
        Label {
            anchors.fill: parent
            anchors.leftMargin: 3
            anchors.rightMargin: 5
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: alignRight ? Text.AlignRight : Text.AlignLeft
            text: caption + (backend.sortColumn === column ? (backend.sortOrder === Qt.AscendingOrder ? " ↑" : " ↓") : "")
            color: backend.sortColumn === column ? theme.accent : "#92929c"
            font.pixelSize: 10
            elide: Text.ElideRight
        }
        MouseArea { anchors.fill: parent; onClicked: backend.sortBy(column) }
    }

    component ActionButton: ToolButton {
        implicitWidth: 28
        implicitHeight: 26
        padding: 3
        display: AbstractButton.IconOnly
        icon.width: 16
        icon.height: 16
        icon.color: "#d1d1d8"
        Material.roundedScale: Math.min(3, theme.radius)
        ToolTip.visible: hovered
        ToolTip.delay: 500
    }

    component CompactButton: Button {
        implicitHeight: 28
        font.pixelSize: 11
        leftPadding: 12
        rightPadding: 12
        topPadding: 0
        bottomPadding: 0
        Material.roundedScale: theme.radius
    }

    function saveChanges() {
        tagEditor.commit()
        return backend.save()
    }

    function runPendingAction() {
        const action = pendingAction
        const path = pendingPath
        pendingAction = ""
        pendingPath = ""
        unsavedDialog.close()
        if (action === "close") {
            allowClose = true
            win.close()
        } else if (action === "folder") {
            backend.chooseDirectory()
        } else if (action === "files") {
            backend.chooseFiles()
        } else if (action === "directory") {
            backend.openDirectoryFromEditor(path)
        }
    }

    function requestOpen(action) {
        tagEditor.commit()
        if (backend.dirtyCount > 0) {
            pendingAction = action
            pendingPath = ""
            unsavedDialog.open()
        } else if (action === "folder") {
            backend.chooseDirectory()
        } else if (action === "files") {
            backend.chooseFiles()
        }
    }

    function requestOpenDirectory(path) {
        tagEditor.commit()
        if (backend.dirtyCount > 0) {
            pendingAction = "directory"
            pendingPath = path
            unsavedDialog.open()
        } else {
            backend.openDirectoryFromEditor(path)
        }
    }

    function requestClose() {
        tagEditor.commit()
        if (backend.dirtyCount > 0) {
            pendingAction = "close"
            unsavedDialog.open()
        } else {
            allowClose = true
            win.close()
        }
    }

    onClosing: function(close) {
        tagEditor.commit()
        if (!allowClose && backend.dirtyCount > 0) {
            close.accepted = false
            pendingAction = "close"
            unsavedDialog.open()
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.editingText; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.editingText && !win.helpVisible; onActivated: win.requestClose() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.editingText && backend.dirtyCount > 0; onActivated: win.saveChanges() }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: win.saveChanges() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: win.requestOpen("folder") }
    Shortcut { sequence: "Ctrl+Shift+O"; context: Qt.ApplicationShortcut; onActivated: win.requestOpen("files") }
    Shortcut { sequences: [StandardKey.SelectAll]; context: Qt.ApplicationShortcut; enabled: !win.editingText; onActivated: backend.selectAll() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.editingText && backend.canUndo; onActivated: backend.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; context: Qt.ApplicationShortcut; enabled: !win.editingText && backend.canRedo; onActivated: backend.redo() }
    Shortcut { sequence: "Ctrl+Y"; context: Qt.ApplicationShortcut; enabled: !win.editingText && backend.canRedo; onActivated: backend.redo() }

    Connections {
        target: translator
        function onChanged() { backend.clearStatus() }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            MenuBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                padding: 0
                spacing: 0
                font.pixelSize: 11
                Menu {
                    title: qsTr("File")
                    CompactMenuItem { text: qsTr("Open folder"); onTriggered: win.requestOpen("folder") }
                    CompactMenuItem { text: qsTr("Open files"); onTriggered: win.requestOpen("files") }
                    MenuSeparator {}
                    CompactMenuItem { text: qsTr("Save"); enabled: backend.dirtyCount > 0; onTriggered: win.saveChanges() }
                    MenuSeparator {}
                    CompactMenuItem { text: qsTr("Quit"); onTriggered: win.requestClose() }
                }
                Menu {
                    title: qsTr("Edit")
                    CompactMenuItem { text: qsTr("Undo"); enabled: backend.canUndo; onTriggered: backend.undo() }
                    CompactMenuItem { text: qsTr("Redo"); enabled: backend.canRedo; onTriggered: backend.redo() }
                    MenuSeparator {}
                    CompactMenuItem { text: qsTr("Select all"); onTriggered: backend.selectAll() }
                }
                Menu {
                    title: qsTr("View")
                    CompactMenuItem { text: qsTr("Filter tracks…"); onTriggered: search.forceActiveFocus() }
                    CompactMenuItem { text: qsTr("Column visibility…"); onTriggered: win.columnSettingsVisible = true }
                    CompactMenuItem { text: qsTr("Keyboard shortcuts"); onTriggered: win.helpVisible = true }
                }
                Menu {
                    title: qsTr("Actions")
                    CompactMenuItem { text: qsTr("Change cover"); enabled: backend.selectedCount > 0; onTriggered: backend.chooseCover() }
                    CompactMenuItem { text: qsTr("Remove cover"); enabled: backend.hasCover; onTriggered: backend.removeCover() }
                }
                Menu {
                    title: qsTr("Help")
                    CompactMenuItem { text: qsTr("Keyboard shortcuts"); onTriggered: win.helpVisible = true }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 31
                color: "#141418"
                border.color: "#25252b"
                border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5
                    spacing: 2
                    ActionButton {
                        icon.source: "qrc:/icons/open-folder.svg"
                        Accessible.name: qsTr("Open folder")
                        ToolTip.text: qsTr("Open folder")
                        onClicked: win.requestOpen("folder")
                    }
                    ActionButton {
                        icon.source: "qrc:/icons/open-files.svg"
                        Accessible.name: qsTr("Open files")
                        ToolTip.text: qsTr("Open files")
                        onClicked: win.requestOpen("files")
                    }
                    ActionButton {
                        icon.source: "qrc:/icons/save.svg"
                        enabled: backend.dirtyCount > 0
                        Accessible.name: qsTr("Save")
                        ToolTip.text: qsTr("Save")
                        onClicked: win.saveChanges()
                    }
                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 17; Layout.leftMargin: 3; Layout.rightMargin: 3; color: "#303037" }
                    ActionButton {
                        icon.source: "qrc:/icons/undo.svg"
                        enabled: backend.canUndo
                        Accessible.name: qsTr("Undo")
                        ToolTip.text: qsTr("Undo")
                        onClicked: backend.undo()
                    }
                    ActionButton {
                        icon.source: "qrc:/icons/redo.svg"
                        enabled: backend.canRedo
                        Accessible.name: qsTr("Redo")
                        ToolTip.text: qsTr("Redo")
                        onClicked: backend.redo()
                    }
                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 17; Layout.leftMargin: 3; Layout.rightMargin: 3; color: "#303037" }
                    ActionButton {
                        icon.source: "qrc:/icons/select-all.svg"
                        enabled: backend.visibleCount > 0
                        Accessible.name: qsTr("Select all")
                        ToolTip.text: qsTr("Select all")
                        onClicked: backend.selectAll()
                    }
                    ActionButton {
                        icon.source: "qrc:/icons/cover.svg"
                        enabled: backend.selectedCount > 0
                        Accessible.name: qsTr("Change cover")
                        ToolTip.text: qsTr("Change cover")
                        onClicked: backend.chooseCover()
                    }
                    ActionButton {
                        icon.source: "qrc:/icons/play.svg"
                        enabled: backend.selectedCount > 0
                        Accessible.name: qsTr("Play")
                        ToolTip.text: qsTr("Play in default player")
                        onClicked: backend.playPath(backend.firstSelectedPath())
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: backend.directory
                        color: "#797984"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                        Layout.preferredWidth: 240
                        horizontalAlignment: Text.AlignRight
                    }
                    ActionButton {
                        icon.source: "qrc:/icons/help.svg"
                        Accessible.name: qsTr("Keyboard shortcuts")
                        ToolTip.text: qsTr("Keyboard shortcuts")
                        onClicked: win.helpVisible = true
                    }
                }
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal
                handle: Rectangle { implicitWidth: 2; color: "#29292f" }

                TagEditor {
                    id: tagEditor
                    objectName: "tagEditor"
                    SplitView.preferredWidth: 300
                    SplitView.minimumWidth: 240
                    SplitView.maximumWidth: 400
                    SplitView.fillHeight: true
                    Material.roundedScale: theme.radius
                    onDirectoryOpenRequested: function(path) { win.requestOpenDirectory(path) }
                }

                Rectangle {
                    SplitView.fillWidth: true
                    SplitView.fillHeight: true
                    SplitView.minimumWidth: 220
                    color: "#101013"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            color: "#17171b"
                            border.color: "#2a2a30"
                            border.width: 1
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                HeaderItem { width: listView.fileColumnWidth; caption: qsTr("Filename"); column: "fileName" }
                                HeaderItem { visible: listView.showPathColumn; width: visible ? 220 : 0; caption: qsTr("Path"); column: "path" }
                                HeaderItem { visible: listView.showTitleColumn; width: visible ? 160 : 0; caption: qsTr("Title"); column: "title" }
                                HeaderItem { visible: listView.showArtistColumn; width: visible ? 130 : 0; caption: qsTr("Artist"); column: "artist" }
                                HeaderItem { visible: listView.showAlbumArtistColumn; width: visible ? 140 : 0; caption: qsTr("Album artist"); column: "albumArtist" }
                                HeaderItem { visible: listView.showAlbumColumn; width: visible ? 140 : 0; caption: qsTr("Album"); column: "album" }
                                HeaderItem { visible: listView.showGenreColumn; width: visible ? 120 : 0; caption: qsTr("Genre"); column: "genre" }
                                HeaderItem { visible: listView.showTrackColumn; width: visible ? 42 : 0; caption: qsTr("#"); column: "trackNumber"; alignRight: true }
                                HeaderItem { visible: listView.showDurationColumn; width: visible ? 50 : 0; caption: qsTr("Time"); column: "duration" }
                                HeaderItem { visible: listView.showFormatColumn; width: visible ? 46 : 0; caption: qsTr("Type"); column: "format" }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            ListView {
                                id: listView
                                objectName: "trackList"
                                anchors.fill: parent
                                clip: true
                                model: backend.model
                                property bool showPathColumn: backend.showPathColumn
                                property bool showTitleColumn: backend.showTitleColumn
                                property bool showArtistColumn: backend.showArtistColumn
                                property bool showAlbumArtistColumn: backend.showAlbumArtistColumn
                                property bool showAlbumColumn: backend.showAlbumColumn
                                property bool showGenreColumn: backend.showGenreColumn
                                property bool showTrackColumn: backend.showTrackColumn
                                property bool showDurationColumn: backend.showDurationColumn
                                property bool showFormatColumn: backend.showFormatColumn
                                property real fileColumnWidth: Math.max(120, width - 8
                                    - (showPathColumn ? 220 : 0) - (showTitleColumn ? 160 : 0)
                                    - (showArtistColumn ? 130 : 0) - (showAlbumArtistColumn ? 140 : 0)
                                    - (showAlbumColumn ? 140 : 0) - (showGenreColumn ? 120 : 0)
                                    - (showTrackColumn ? 42 : 0)
                                    - (showDurationColumn ? 50 : 0) - (showFormatColumn ? 46 : 0))
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: ScrollBar {}
                                property int editingRow: -1
                                delegate: TrackRow {
                                    editing: listView.editingRow === index
                                    fileColumnWidth: listView.fileColumnWidth
                                    showPath: listView.showPathColumn
                                    showTitle: listView.showTitleColumn
                                    showArtist: listView.showArtistColumn
                                    showAlbumArtist: listView.showAlbumArtistColumn
                                    showAlbum: listView.showAlbumColumn
                                    showGenre: listView.showGenreColumn
                                    showTrack: listView.showTrackColumn
                                    showDuration: listView.showDurationColumn
                                    showFormat: listView.showFormatColumn
                                    onRowClicked: function(index, control, shift) {
                                        // Selecting another track leaves any inline rename.
                                        listView.editingRow = -1
                                        listView.forceActiveFocus()
                                        backend.selectRow(index, control, shift)
                                    }
                                    onRenameRequested: function(index) {
                                        listView.editingRow = index
                                    }
                                    onRenameFinished: function(index) {
                                        if (listView.editingRow === index)
                                            listView.editingRow = -1
                                    }
                                }
                            }

                            Connections {
                                target: backend
                                function onSelectionChanged() { listView.editingRow = -1 }
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                width: Math.min(parent.width - 36, 380)
                                visible: backend.trackCount === 0 && !backend.loading
                                spacing: 9
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Your music, properly tagged.")
                                    color: "#eeeeF2"
                                    font.pixelSize: 18
                                    font.weight: Font.DemiBold
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Open a folder to edit MP3, FLAC, M4A, Ogg, Opus, WAV, and other audio metadata natively.")
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    color: "#888893"
                                    font.pixelSize: 11
                                }
                                CompactButton {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 180
                                    text: qsTr("Choose music folder")
                                    highlighted: true
                                    onClicked: win.requestOpen("folder")
                                }
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: backend.trackCount > 0 && backend.visibleCount === 0
                                text: qsTr("No tracks match your filter.")
                                color: "#888893"
                                font.pixelSize: 11
                            }
                            BusyIndicator { anchors.centerIn: parent; running: backend.loading; visible: backend.loading; implicitWidth: 34; implicitHeight: 34 }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 29
                color: "#141418"
                border.color: "#29292f"
                border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 7
                    Label { text: qsTr("Filter"); color: "#a0a0aa"; font.pixelSize: 11 }
                    TextField {
                        id: search
                        Layout.fillWidth: true
                        implicitHeight: 22
                        font.pixelSize: 11
                        leftPadding: 6
                        rightPadding: 6
                        topPadding: 0
                        bottomPadding: 0
                        placeholderText: ""
                        text: backend.filterText
                        onTextEdited: backend.filterText = text
                        selectByMouse: true
                        color: "#e2e2e7"
                        background: Rectangle { radius: Math.min(2, theme.radius); color: "#0f0f12"; border.color: search.activeFocus ? theme.accent : "#323239"; border.width: 1 }
                        Keys.onEscapePressed: { search.text = ""; backend.filterText = ""; search.focus = false }
                    }
                    Label {
                        text: backend.filterText.length > 0
                            ? qsTr("%1 of %2 tracks").arg(backend.visibleCount).arg(backend.trackCount)
                            : qsTr("%n tracks", "", backend.trackCount)
                        color: "#858590"
                        font.pixelSize: 10
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 23
                color: "#101013"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 12
                    Label {
                        Layout.fillWidth: true
                        text: backend.statusMessage || qsTr("Ready")
                        color: backend.statusIsError ? "#ff8585" : "#858590"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    Label { visible: backend.dirtyCount > 0; text: qsTr("%n unsaved", "", backend.dirtyCount); color: theme.accent; font.pixelSize: 10 }
                    Label { visible: backend.selectedCount > 0; text: qsTr("%n selected", "", backend.selectedCount); color: "#858590"; font.pixelSize: 10 }
                }
            }
        }
    }

    Dialog {
        id: columnSettingsDialog
        title: qsTr("Column visibility")
        visible: win.columnSettingsVisible
        modal: true
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        closePolicy: Popup.CloseOnEscape
        font.pixelSize: 11
        Material.roundedScale: theme.radius
        onRejected: win.columnSettingsVisible = false
        contentItem: ColumnLayout {
            spacing: 4
            Label {
                Layout.fillWidth: true
                text: qsTr("Show or hide columns in the track list.")
                wrapMode: Text.Wrap
                color: "#858590"
                font.pixelSize: 11
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 6
                    Label { Layout.fillWidth: true; text: qsTr("Path"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showPathColumn
                        onToggled: backend.setColumnVisible("path", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Title"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showTitleColumn
                        onToggled: backend.setColumnVisible("title", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Artist"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showArtistColumn
                        onToggled: backend.setColumnVisible("artist", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Album artist"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showAlbumArtistColumn
                        onToggled: backend.setColumnVisible("albumArtist", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Album"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showAlbumColumn
                        onToggled: backend.setColumnVisible("album", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Genre"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showGenreColumn
                        onToggled: backend.setColumnVisible("genre", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("#"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showTrackColumn
                        onToggled: backend.setColumnVisible("trackNumber", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Time"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showDurationColumn
                        onToggled: backend.setColumnVisible("duration", checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    Label { Layout.fillWidth: true; text: qsTr("Type"); color: "#d5d5dc"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                    CheckBox {
                        checked: backend.showFormatColumn
                        onToggled: backend.setColumnVisible("format", checked)
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12
                spacing: 7
                Item { Layout.fillWidth: true }
                CompactButton { text: qsTr("Close"); flat: true; implicitHeight: 26; onClicked: win.columnSettingsVisible = false }
            }
        }
    }

    Dialog {
        id: unsavedDialog
        title: qsTr("Unsaved changes")
        modal: true
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        closePolicy: Popup.CloseOnEscape
        font.pixelSize: 11
        Material.roundedScale: theme.radius
        onRejected: { win.pendingAction = ""; win.pendingPath = "" }
        contentItem: ColumnLayout {
            spacing: 16
            Label {
                Layout.fillWidth: true
                text: qsTr("Save your tag changes before continuing? Discarding them cannot be undone.")
                wrapMode: Text.Wrap
                color: "#dedee4"
                font.pixelSize: 11
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 7
                CompactButton { text: qsTr("Cancel"); onClicked: { win.pendingAction = ""; win.pendingPath = ""; unsavedDialog.close() } }
                Item { Layout.fillWidth: true }
                CompactButton { text: qsTr("Discard"); onClicked: { backend.discardChanges(); win.runPendingAction() } }
                CompactButton { text: qsTr("Save changes"); highlighted: true; onClicked: if (win.saveChanges()) win.runPendingAction() }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 100
        visible: win.helpVisible
        color: "#d9000000"
        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(490, parent.width - 32)
            height: Math.min(420, parent.height - 32)
            radius: theme.radius
            color: "#17171c"
            border.color: "#36363f"
            border.width: 1
            MouseArea { anchors.fill: parent }
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 13
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Keyboard shortcuts"); color: "#f3f3f6"; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    CompactButton { text: qsTr("Close"); flat: true; implicitHeight: 26; onClicked: win.helpVisible = false }
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 20
                    rowSpacing: 9
                    Label { text: "Ctrl+O"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Open folder"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "Ctrl+Shift+O"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Open audio files"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "Space / Ctrl+S"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Save all changes"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "Ctrl+A"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Select all visible tracks"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "Ctrl+Z / Ctrl+Shift+Z"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Undo / redo an edit"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "Shift+Click / Ctrl+Click"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Select a range / toggle a track"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "?"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Show or hide this guide"); color: "#d5d5dc"; font.pixelSize: 11 }
                    Label { text: "Q"; color: theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: qsTr("Quit Tagr"); color: "#d5d5dc"; font.pixelSize: 11 }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Enter commits a field. Shortcuts that conflict with typing are paused while you edit text.")
                    wrapMode: Text.Wrap
                    color: "#858590"
                    font.pixelSize: 11
                }
            }
        }
    }
}
