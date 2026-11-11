import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

FocusScope {
    id: editor
    signal directoryOpenRequested(string path)

    component FieldLabel: Label {
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        color: "#9b9ba5"
        font.pixelSize: 11
        verticalAlignment: Text.AlignBottom
    }

    component EditField: TextField {
        id: field
        property string fieldName
        property bool edited: false
        property bool committing: false
        property bool mixed: false
        property bool locationField: false
        enabled: locationField || backend.selectedCount > 0

        function chooseValue(value) {
            if (locationField) {
                text = value
                edited = true
                valueMenu.close()
                commitLocation()
                return
            }
            if (readOnly)
                return
            text = value
            edited = true
            commit()
            valueMenu.close()
        }

        function addMenuValue(display, value, keep) {
            var item = valueItemComponent.createObject(null, { text: display })
            item.triggered.connect(function() {
                if (keep)
                    valueMenu.close()
                else
                    chooseValue(value)
            })
            valueMenu.addItem(item)
        }

        function openValues() {
            if (readOnly)
                return
            while (valueMenu.count > 0)
                valueMenu.takeItem(0).destroy()
            if (locationField) {
                const directories = backend.recentDirectories
                for (var index = 0; index < directories.length; ++index)
                    addMenuValue(directories[index], directories[index], false)
                if (valueMenu.count > 0)
                    valueMenu.open()
                return
            }
            if (backend.selectedCount < 2)
                return
            if (mixed)
                addMenuValue(qsTr("< keep >"), "", true)
            const values = backend.fieldValues(fieldName)
            for (var index = 0; index < values.length; ++index) {
                const value = values[index]
                addMenuValue(value.length > 0 ? value : qsTr("< blank >"), value, false)
            }
            valueMenu.open()
        }

        function commit() {
            if (locationField)
                return
            if (!edited)
                return
            committing = true
            backend.setField(fieldName, text)
            edited = false
            committing = false
        }

        function commitLocation() {
            if (!locationField || !edited)
                return
            const target = text
            edited = false
            focus = false
            text = Qt.binding(function() { return backend.directory })
            if (target !== backend.directory)
                editor.directoryOpenRequested(target)
        }
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        implicitHeight: 26
        placeholderText: ""
        selectByMouse: true
        color: "#ededf1"
        font.pixelSize: 11
        leftPadding: 6
        rightPadding: valueButton.visible ? 22 : 6
        topPadding: 0
        bottomPadding: 0
        Material.roundedScale: Math.min(2, theme.radius)
        background: Rectangle {
            radius: Math.min(2, theme.radius)
            color: field.enabled ? "#17171c" : "#131317"
            border.color: field.activeFocus ? theme.accent : "#33333b"
            border.width: 1
        }
        Component {
            id: valueItemComponent
            MenuItem { implicitHeight: 25; font.pixelSize: 11 }
        }
        Menu {
            id: valueMenu
            objectName: "valueMenu"
            parent: field
            x: 0
            y: field.height
            width: Math.max(180, field.width)
            padding: 3
        }
        ToolButton {
            id: valueButton
            objectName: "valueButton"
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 22
            padding: 0
            display: AbstractButton.IconOnly
            icon.source: "qrc:/icons/chevron-down.svg"
            icon.width: 11
            icon.height: 11
            icon.color: "#92929c"
            visible: field.locationField ? backend.recentDirectories.length > 0 : backend.selectedCount > 1 && !field.readOnly
            Accessible.name: field.locationField ? qsTr("Recent directories") : qsTr("Show selected values")
            ToolTip.visible: hovered
            ToolTip.text: field.locationField ? qsTr("Recent directories") : qsTr("Show selected values")
            onClicked: field.openValues()
            background: Rectangle {
                color: valueButton.hovered ? "#29292f" : "transparent"
                radius: Math.min(2, theme.radius)
            }
        }
        Label {
            objectName: "keepHint"
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: valueButton.visible ? valueButton.left : parent.right
            anchors.rightMargin: valueButton.visible ? 2 : 4
            anchors.verticalCenter: parent.verticalCenter
            visible: field.mixed && !field.edited && field.text.length === 0
            text: qsTr("< keep >")
            color: "#92929c"
            font.pixelSize: 10
            elide: Text.ElideRight
        }
        onTextChanged: if (activeFocus && !committing && !readOnly) edited = true
        onEditingFinished: if (!locationField) commit()
        onAccepted: if (locationField) commitLocation()
        onActiveFocusChanged: {
            if (locationField && !activeFocus) {
                edited = false
                text = Qt.binding(function() { return backend.directory })
            }
        }
        Connections {
            target: backend
            function onDirectoryChanged() {
                if (field.locationField) {
                    field.committing = true
                    field.text = Qt.binding(function() { return backend.directory })
                    field.edited = false
                    field.committing = false
                }
            }
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Down && event.modifiers & Qt.AltModifier) {
                field.openValues()
                event.accepted = true
            }
        }
    }

    component CoverButton: Button {
        id: coverButton
        implicitHeight: 25
        font.pixelSize: 10
        contentItem: Label {
            text: coverButton.text
            color: coverButton.enabled ? "#d8d8df" : "#65656e"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Math.min(2, theme.radius)
            color: coverButton.pressed ? "#2b2b33" : coverButton.hovered ? "#25252c" : "#1d1d23"
            border.color: "#383840"
            border.width: 1
        }
    }

    function commitItem(item) {
        if (item.fieldName !== undefined && item.edited)
            item.commit()
        for (var index = 0; index < item.children.length; ++index)
            commitItem(item.children[index])
    }

    function commit() {
        commitItem(fieldsColumn)
    }

    Rectangle {
        anchors.fill: parent
        color: "#121216"
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        // Keep the overlay scrollbar off the right edge of the fields.
        leftPadding: 0
        topPadding: 0
        bottomPadding: 0
        rightPadding: 16
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: fieldsColumn
            width: scroll.availableWidth
            spacing: 3

            Item { Layout.preferredHeight: 4 }

            FieldLabel { text: qsTr("Title") }
            EditField {
                fieldName: "title"
                text: { backend.formRevision; return backend.fieldMixed("title") ? "" : backend.title }
                mixed: { backend.formRevision; return backend.fieldMixed("title") }
            }
            FieldLabel { text: qsTr("Artist") }
            EditField {
                fieldName: "artist"
                text: { backend.formRevision; return backend.fieldMixed("artist") ? "" : backend.artist }
                mixed: { backend.formRevision; return backend.fieldMixed("artist") }
            }
            FieldLabel { text: qsTr("Album") }
            EditField {
                fieldName: "album"
                text: { backend.formRevision; return backend.fieldMixed("album") ? "" : backend.album }
                mixed: { backend.formRevision; return backend.fieldMixed("album") }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 1
                spacing: 6
                ColumnLayout {
                    Layout.preferredWidth: 80
                    Layout.minimumWidth: 80
                    Layout.maximumWidth: 80
                    spacing: 3
                    FieldLabel { Layout.leftMargin: 0; Layout.rightMargin: 0; text: qsTr("Year") }
                    EditField {
                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        Layout.preferredWidth: 80
                        Layout.minimumWidth: 80
                        Layout.maximumWidth: 80
                        fieldName: "year"
                        text: { backend.formRevision; return backend.fieldMixed("year") ? "" : backend.yearText }
                        mixed: { backend.formRevision; return backend.fieldMixed("year") }
                        validator: IntValidator { bottom: 0; top: 9999 }
                        inputMethodHints: Qt.ImhDigitsOnly
                    }
                }
                ColumnLayout {
                    Layout.preferredWidth: 80
                    Layout.minimumWidth: 80
                    Layout.maximumWidth: 80
                    spacing: 3
                    FieldLabel { Layout.leftMargin: 0; Layout.rightMargin: 0; text: qsTr("Track") }
                    EditField {
                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        Layout.preferredWidth: 80
                        Layout.minimumWidth: 80
                        Layout.maximumWidth: 80
                        fieldName: "track"
                        text: { backend.formRevision; return backend.fieldMixed("track") ? "" : backend.trackText }
                        mixed: { backend.formRevision; return backend.fieldMixed("track") }
                        validator: IntValidator { bottom: 0; top: 9999 }
                        inputMethodHints: Qt.ImhDigitsOnly
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 52
                    spacing: 3
                    FieldLabel { Layout.leftMargin: 0; Layout.rightMargin: 0; text: qsTr("Genre") }
                    EditField {
                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        fieldName: "genre"
                        text: { backend.formRevision; return backend.fieldMixed("genre") ? "" : backend.genre }
                        mixed: { backend.formRevision; return backend.fieldMixed("genre") }
                    }
                }
            }

            FieldLabel { Layout.topMargin: 3; text: qsTr("Comment") }
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.preferredHeight: 54
                radius: Math.min(2, theme.radius)
                color: "#17171c"
                border.color: commentArea.activeFocus ? theme.accent : "#33333b"
                border.width: 1
                TextArea {
                    id: commentArea
                    enabled: backend.selectedCount > 0
                    anchors.fill: parent
                    anchors.margins: 3
                    property string fieldName: "comment"
                    property bool edited: false
                    property bool committing: false
                    function commit() {
                        if (!edited)
                            return
                        committing = true
                        backend.setField(fieldName, text)
                        edited = false
                        committing = false
                    }
                    function chooseValue(value) {
                        text = value
                        edited = true
                        commit()
                        commentValueMenu.close()
                    }
                    function openValues() {
                        while (commentValueMenu.count > 0)
                            commentValueMenu.takeItem(0).destroy()
                        if (backend.fieldMixed("comment")) {
                            var keep = commentValueItem.createObject(null, { text: qsTr("< keep >") })
                            keep.triggered.connect(function() { commentValueMenu.close() })
                            commentValueMenu.addItem(keep)
                        }
                        const values = backend.fieldValues("comment")
                        for (var index = 0; index < values.length; ++index) {
                            const value = values[index]
                            var item = commentValueItem.createObject(null, { text: value.length > 0 ? value : qsTr("< blank >") })
                            item.triggered.connect(function() { chooseValue(value) })
                            commentValueMenu.addItem(item)
                        }
                        commentValueMenu.open()
                    }
                    text: { backend.formRevision; return backend.fieldMixed("comment") ? "" : backend.comment }
                    placeholderText: ""
                    color: "#ededf1"
                    font.pixelSize: 11
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    rightPadding: backend.selectedCount > 1 ? 26 : 4
                    Material.roundedScale: Math.min(2, theme.radius)
                    onTextChanged: if (activeFocus && !committing) edited = true
                    onActiveFocusChanged: if (!activeFocus) commit()
                }
                Component {
                    id: commentValueItem
                    MenuItem { implicitHeight: 25; font.pixelSize: 11 }
                }
                Menu {
                    id: commentValueMenu
                    parent: commentArea
                    x: 0
                    y: commentArea.height
                    width: Math.max(180, commentArea.width)
                    padding: 3
                }
                ToolButton {
                    id: commentValueButton
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 2
                    width: 18
                    height: 22
                    padding: 0
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/icons/chevron-down.svg"
                    icon.width: 11
                    icon.height: 11
                    icon.color: "#92929c"
                    visible: backend.selectedCount > 1
                    Accessible.name: qsTr("Show selected values")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Show selected values")
                    onClicked: commentArea.openValues()
                    background: Rectangle { color: commentValueButton.hovered ? "#29292f" : "transparent"; radius: Math.min(2, theme.radius) }
                }
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 7
                    anchors.top: parent.top
                    anchors.topMargin: 5
                    visible: { backend.formRevision; return backend.fieldMixed("comment") && !commentArea.edited && commentArea.text.length === 0 }
                    text: qsTr("< keep >")
                    color: "#70707a"
                    font.pixelSize: 11
                }
            }

            FieldLabel { Layout.topMargin: 3; text: qsTr("Album artist") }
            EditField {
                fieldName: "albumArtist"
                text: { backend.formRevision; return backend.fieldMixed("albumArtist") ? "" : backend.albumArtist }
                mixed: { backend.formRevision; return backend.fieldMixed("albumArtist") }
            }
            FieldLabel { text: qsTr("Disc") }
            EditField {
                fieldName: "disc"
                text: { backend.formRevision; return backend.fieldMixed("disc") ? "" : backend.discText }
                mixed: { backend.formRevision; return backend.fieldMixed("disc") }
                validator: IntValidator { bottom: 0; top: 9999 }
                inputMethodHints: Qt.ImhDigitsOnly
            }
            FieldLabel { text: qsTr("Directory") }
            EditField {
                fieldName: "directory"
                locationField: true
                text: backend.directory
                selectByMouse: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 10
                spacing: 10
                Rectangle {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 120
                    radius: Math.min(2, theme.radius)
                    color: "#0b0b0e"
                    border.color: "#303037"
                    border.width: 1
                    clip: true
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: backend.hasCover ? "image://tagrcover/cover?revision=" + backend.coverRevision : ""
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !backend.hasCover
                        text: qsTr("No cover art")
                        color: "#666671"
                        font.pixelSize: 10
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 5
                    Label {
                        Layout.fillWidth: true
                        visible: backend.hasCover
                        text: backend.coverInfo
                        color: "#92929c"
                        font.pixelSize: 10
                        wrapMode: Text.WrapAnywhere
                    }
                    Item { Layout.fillHeight: true }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                spacing: 6
                CoverButton {
                    objectName: "changeCoverButton"
                    Layout.fillWidth: true
                    implicitHeight: 25
                    text: qsTr("Change cover")
                    font.pixelSize: 10
                    enabled: backend.selectedCount > 0
                    Material.roundedScale: Math.min(2, theme.radius)
                    onClicked: backend.chooseCover()
                }
                CoverButton {
                    implicitHeight: 25
                    text: qsTr("Remove")
                    font.pixelSize: 10
                    enabled: backend.hasCover
                    Material.roundedScale: Math.min(2, theme.radius)
                    onClicked: backend.removeCover()
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                spacing: 5
                FieldLabel { Layout.fillWidth: true; text: qsTr("MusicBrainz release ID") }
                EditField {
                    fieldName: "musicbrainzReleaseId"
                    text: { backend.formRevision; return backend.fieldMixed("musicbrainzReleaseId") ? "" : backend.musicbrainzReleaseId() }
                    mixed: { backend.formRevision; return backend.fieldMixed("musicbrainzReleaseId") }
                }
                FieldLabel { Layout.fillWidth: true; text: qsTr("MusicBrainz album ID") }
                EditField {
                    fieldName: "musicbrainzAlbumId"
                    text: { backend.formRevision; return backend.fieldMixed("musicbrainzAlbumId") ? "" : backend.musicbrainzAlbumId() }
                    mixed: { backend.formRevision; return backend.fieldMixed("musicbrainzAlbumId") }
                }
                FieldLabel { Layout.fillWidth: true; text: qsTr("MusicBrainz album artist ID") }
                EditField {
                    fieldName: "musicbrainzAlbumArtistId"
                    text: { backend.formRevision; return backend.fieldMixed("musicbrainzAlbumArtistId") ? "" : backend.musicbrainzAlbumArtistId() }
                    mixed: { backend.formRevision; return backend.fieldMixed("musicbrainzAlbumArtistId") }
                }
                FieldLabel { Layout.fillWidth: true; text: qsTr("MusicBrainz artist ID") }
                EditField {
                    fieldName: "musicbrainzArtistId"
                    text: { backend.formRevision; return backend.fieldMixed("musicbrainzArtistId") ? "" : backend.musicbrainzArtistId() }
                    mixed: { backend.formRevision; return backend.fieldMixed("musicbrainzArtistId") }
                }
                FieldLabel { Layout.fillWidth: true; text: qsTr("MusicBrainz track ID") }
                EditField {
                    fieldName: "musicbrainzTrackId"
                    text: { backend.formRevision; return backend.fieldMixed("musicbrainzTrackId") ? "" : backend.musicbrainzTrackId() }
                    mixed: { backend.formRevision; return backend.fieldMixed("musicbrainzTrackId") }
                }
                FieldLabel { Layout.fillWidth: true; text: qsTr("MusicBrainz release group ID") }
                EditField {
                    fieldName: "musicbrainzReleaseGroupId"
                    text: { backend.formRevision; return backend.fieldMixed("musicbrainzReleaseGroupId") ? "" : backend.musicbrainzReleaseGroupId() }
                    mixed: { backend.formRevision; return backend.fieldMixed("musicbrainzReleaseGroupId") }
                }
            }
            Item { Layout.preferredHeight: 10 }
        }
    }
}
