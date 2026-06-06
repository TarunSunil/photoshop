import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1440
    height: 920
    visible: true
    title: "LumenForge"
    color: "#15171b"

    property real zoom: 1.0

    FileDialog {
        id: openImageDialog
        title: "Open image"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp)", "All files (*)"]
        onAccepted: documentController.openImage(selectedFile)
    }

    FileDialog {
        id: openProjectDialog
        title: "Open project"
        nameFilters: ["LumenForge project (*.lfproj)", "All files (*)"]
        onAccepted: documentController.loadProject(selectedFile)
    }

    FileDialog {
        id: saveProjectDialog
        title: "Save project"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "lfproj"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.saveProject(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Export image"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: ["PNG (*.png)", "JPEG (*.jpg *.jpeg)", "WebP (*.webp)"]
        onAccepted: documentController.exportImage(selectedFile)
    }

    Shortcut { sequence: StandardKey.Open; onActivated: openImageDialog.open() }
    Shortcut { sequence: StandardKey.Save; onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+E"; onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+0"; onActivated: root.zoom = 1.0 }
    Shortcut { sequence: "Ctrl++"; onActivated: root.zoom = Math.min(4.0, root.zoom + 0.1) }
    Shortcut { sequence: "Ctrl+-"; onActivated: root.zoom = Math.max(0.1, root.zoom - 0.1) }
    Shortcut { sequence: StandardKey.Undo; onActivated: documentController.undo() }
    Shortcut { sequence: StandardKey.Redo; onActivated: documentController.redo() }

    header: ToolBar {
        height: 52
        background: Rectangle { color: "#1e2228" }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Label {
                text: "LumenForge"
                color: "#f4f7fb"
                font.pixelSize: 18
                font.bold: true
            }

            Label {
                text: documentController.sourceName
                color: "#98a2b3"
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            Button { text: "Open"; onClicked: openImageDialog.open() }
            Button { text: "Project"; onClicked: openProjectDialog.open() }
            Button { text: "Save"; enabled: documentController.hasDocument; onClicked: saveProjectDialog.open() }
            Button { text: "Export"; enabled: documentController.hasDocument; onClicked: exportDialog.open() }
        }
    }

    footer: Rectangle {
        height: 116
        color: "#1b1f25"
        border.color: "#2b313a"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Repeater {
                model: ["History", "Layers", "Masks", "Filmstrip"]
                delegate: Rectangle {
                    Layout.preferredWidth: 180
                    Layout.fillHeight: true
                    radius: 6
                    color: index === 0 ? "#252b33" : "#20252c"
                    border.color: "#353c46"

                    Label {
                        anchors.centerIn: parent
                        text: modelData
                        color: "#cbd5e1"
                        font.pixelSize: 13
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 72
            Layout.fillHeight: true
            color: "#181b20"
            border.color: "#272d35"

            ColumnLayout {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 14
                spacing: 10

                Repeater {
                    model: [
                        { icon: "M", tip: "Move" },
                        { icon: "C", tip: "Crop" },
                        { icon: "B", tip: "Brush mask" },
                        { icon: "E", tip: "Erase mask" },
                        { icon: "G", tip: "Gradient mask" },
                        { icon: "R", tip: "Radial mask" }
                    ]
                    delegate: Button {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 40
                        text: modelData.icon
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.tip
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#101215"

            Flickable {
                id: canvasFlick
                anchors.fill: parent
                contentWidth: Math.max(width, imagePreview.width * root.zoom)
                contentHeight: Math.max(height, imagePreview.height * root.zoom)
                clip: true

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.max(360, imagePreview.width * root.zoom + 72)
                    height: Math.max(260, imagePreview.height * root.zoom + 72)
                    color: "#0c0e11"
                    border.color: "#2d333c"

                    Image {
                        id: imagePreview
                        anchors.centerIn: parent
                        source: documentController.imageUrl
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        width: sourceSize.width > 0 ? sourceSize.width * root.zoom : 0
                        height: sourceSize.height > 0 ? sourceSize.height * root.zoom : 0
                        asynchronous: true
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !documentController.hasDocument
                        text: "Open an image to begin"
                        color: "#d0d5dd"
                        font.pixelSize: 22
                    }
                }
            }

            Row {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 18
                spacing: 8

                Button { text: "Fit"; enabled: documentController.hasDocument; onClicked: root.zoom = 0.6 }
                Button { text: "100%"; enabled: documentController.hasDocument; onClicked: root.zoom = 1.0 }
                Button { text: "-"; enabled: documentController.hasDocument; onClicked: root.zoom = Math.max(0.1, root.zoom - 0.1) }
                Button { text: "+"; enabled: documentController.hasDocument; onClicked: root.zoom = Math.min(4.0, root.zoom + 0.1) }
            }
        }

        Rectangle {
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            color: "#1a1e24"
            border.color: "#2c333d"

            ScrollView {
                anchors.fill: parent

                ColumnLayout {
                    width: parent.width
                    spacing: 16
                    anchors.margins: 18

                    Label {
                        text: "Adjustments"
                        color: "#f2f4f7"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.leftMargin: 18
                        Layout.topMargin: 18
                    }

                    Label {
                        text: "Transform"
                        color: "#f2f4f7"
                        font.pixelSize: 16
                        font.bold: true
                        Layout.leftMargin: 18
                    }

                    GridLayout {
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 8

                        Button {
                            text: "Rotate left"
                            enabled: documentController.hasDocument
                            Layout.fillWidth: true
                            onClicked: documentController.rotateCounterClockwise()
                        }

                        Button {
                            text: "Rotate right"
                            enabled: documentController.hasDocument
                            Layout.fillWidth: true
                            onClicked: documentController.rotateClockwise()
                        }

                        Button {
                            text: "Flip H"
                            enabled: documentController.hasDocument
                            Layout.fillWidth: true
                            onClicked: documentController.flipHorizontal()
                        }

                        Button {
                            text: "Flip V"
                            enabled: documentController.hasDocument
                            Layout.fillWidth: true
                            onClicked: documentController.flipVertical()
                        }
                    }

                    RowLayout {
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "Undo"
                            enabled: documentController.canUndo
                            Layout.fillWidth: true
                            onClicked: documentController.undo()
                        }

                        Button {
                            text: "Redo"
                            enabled: documentController.canRedo
                            Layout.fillWidth: true
                            onClicked: documentController.redo()
                        }
                    }

                    AdjustmentSlider {
                        label: "Exposure"
                        from: -3
                        to: 3
                        value: documentController.exposure
                        onMoved: (nextValue) => documentController.exposure = nextValue
                    }

                    AdjustmentSlider {
                        label: "Contrast"
                        from: -100
                        to: 100
                        value: documentController.contrast
                        onMoved: (nextValue) => documentController.contrast = nextValue
                    }

                    AdjustmentSlider {
                        label: "Saturation"
                        from: -100
                        to: 100
                        value: documentController.saturation
                        onMoved: (nextValue) => documentController.saturation = nextValue
                    }

                    AdjustmentSlider {
                        label: "Temperature"
                        from: -100
                        to: 100
                        value: documentController.temperature
                        onMoved: (nextValue) => documentController.temperature = nextValue
                    }

                    AdjustmentSlider {
                        label: "Tint"
                        from: -100
                        to: 100
                        value: documentController.tint
                        onMoved: (nextValue) => documentController.tint = nextValue
                    }

                    Button {
                        text: "Reset"
                        enabled: documentController.hasDocument
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        Layout.fillWidth: true
                        onClicked: documentController.resetAdjustments()
                    }

                    Label {
                        text: "AI tools"
                        color: "#f2f4f7"
                        font.pixelSize: 16
                        font.bold: true
                        Layout.leftMargin: 18
                        Layout.topMargin: 12
                    }

                    Repeater {
                        model: ["Subject mask", "Background mask", "Object removal", "Upscale"]
                        delegate: Button {
                            text: modelData
                            enabled: false
                            Layout.leftMargin: 18
                            Layout.rightMargin: 18
                            Layout.fillWidth: true
                        }
                    }

                    Item { Layout.preferredHeight: 24 }
                }
            }
        }
    }

    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real from: 0
        property real to: 1
        property real value: 0
        signal moved(real nextValue)

        Layout.leftMargin: 18
        Layout.rightMargin: 18
        Layout.fillWidth: true
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label { text: sliderRoot.label; color: "#d0d5dd"; Layout.fillWidth: true }
            Label { text: Number(slider.value).toFixed(sliderRoot.to <= 3 ? 2 : 0); color: "#98a2b3" }
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            from: sliderRoot.from
            to: sliderRoot.to
            value: sliderRoot.value
            enabled: documentController.hasDocument
            onMoved: sliderRoot.moved(value)
        }
    }
}
