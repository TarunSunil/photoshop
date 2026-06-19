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
    property real brushRadius: 50
    function fitZoom() {
        if (imagePreview.sourceSize.width <= 0) return 1.0
        return Math.min(
            canvasFlick.width  / imagePreview.sourceSize.width,
            canvasFlick.height / imagePreview.sourceSize.height) * 0.95
    }
    FileDialog {
        id: openImageDialog
        title: "Open image"
        nameFilters: [
            "Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp " +
            "*.cr2 *.cr3 *.nef *.arw *.dng *.raf *.orf *.rw2)",
            "All files (*)"
        ]
        onAccepted: documentController.openImage(selectedFile)
    }
    FileDialog {
        id: openProjectDialog; title: "Open project"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.loadProject(selectedFile)
    }
    FileDialog {
        id: saveProjectDialog; title: "Save project"
        fileMode: FileDialog.SaveFile; defaultSuffix: "lfproj"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.saveProject(selectedFile)
    }
    FileDialog {
        id: exportDialog; title: "Export image"
        fileMode: FileDialog.SaveFile; defaultSuffix: "png"
        nameFilters: ["PNG (*.png)", "JPEG (*.jpg)", "WebP (*.webp)"]
        onAccepted: documentController.exportImage(selectedFile)
    }
    FileDialog {
        id: addLayerDialog; title: "Add image layer"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp)"]
        onAccepted: documentController.addImageLayer(selectedFile)
    }
    Dialog {
        id: recoveryDialog
        title: "Recover unsaved work?"
        modal: true
        visible: documentController.hasPendingRecovery
        anchors.centerIn: parent
        Label { text: "An autosaved project was found. Recover it?" }
        footer: DialogButtonBox {
            Button { text: "Recover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { documentController.recoverProject(); recoveryDialog.close() } }
            Button { text: "Discard"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { documentController.discardRecovery(); recoveryDialog.close() } }
        }
    }
    Shortcut { sequence: StandardKey.Open;  onActivated: openImageDialog.open() }
    Shortcut { sequence: StandardKey.Save;  onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+E";          onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+0";          onActivated: root.zoom = 1.0 }
    Shortcut { sequence: "Ctrl++";          onActivated: root.zoom = Math.min(4.0, root.zoom+0.1) }
    Shortcut { sequence: "Ctrl+-";          onActivated: root.zoom = Math.max(0.1, root.zoom-0.1) }
    Shortcut { sequence: StandardKey.Undo;  onActivated: documentController.undo() }
    Shortcut { sequence: StandardKey.Redo;  onActivated: documentController.redo() }
    Shortcut { sequence: "\\\\";             onActivated: documentController.showOriginal = !documentController.showOriginal }
    Shortcut { sequence: "Escape";
        onActivated: if (documentController.activeTool > 0) documentController.activeTool = 0 }
    header: ToolBar {
        height: 52
        background: Rectangle { color: "#1e2228" }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
            Label { text: "LumenForge"; color: "#f4f7fb"; font.pixelSize: 18; font.bold: true }
            Label { text: documentController.sourceName; color: "#98a2b3"
                elide: Text.ElideMiddle; Layout.fillWidth: true }
            Button { text: "Open";    onClicked: openImageDialog.open() }
            Button { text: "Project"; onClicked: openProjectDialog.open() }
            Button { text: "Save";    enabled: documentController.hasDocument
                onClicked: saveProjectDialog.open() }
            Button { text: "Export";  enabled: documentController.hasDocument
                onClicked: exportDialog.open() }
        }
    }
    footer: Rectangle {
        height: 120
        color: "#1b1f25"
        border.color: "#2b313a"
        RowLayout {
            anchors.fill: parent; anchors.margins: 8; spacing: 8
            Rectangle {
                Layout.preferredWidth: 320; Layout.fillHeight: true
                color: "#20252c"; radius: 6; border.color: "#353c46"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 6; spacing: 4
                    RowLayout {
                        spacing: 6
                        Label { text: "Layers"; color: "#cbd5e1"; font.pixelSize: 12; font.bold: true
                            Layout.fillWidth: true }
                        Button { text: "+"; implicitWidth: 28; implicitHeight: 24
                            enabled: documentController.hasDocument
                            onClicked: addLayerDialog.open() }
                    }
                    ListView {
                        id: layerList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: documentController.layerModel
                        clip: true
                        delegate: Rectangle {
                            width: layerList.width; height: 28
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 4; spacing: 4
                                Button { text: modelData.visible ? "👁" : "○"
                                    implicitWidth: 24; implicitHeight: 22; flat: true
                                    onClicked: documentController.setLayerVisible(modelData.id, !modelData.visible) }
                                Label { text: modelData.name; color: "#cbd5e1"
                                    font.pixelSize: 11; Layout.fillWidth: true
                                    elide: Text.ElideRight }
                                Slider { from: 0; to: 1; value: modelData.opacity
                                    implicitWidth: 60; implicitHeight: 22
                                    onMoved: documentController.setLayerOpacity(modelData.id, value) }
                                Button { text: "✕"; implicitWidth: 24; implicitHeight: 22; flat: true
                                    onClicked: documentController.deleteLayer(modelData.id) }
                            }
                        }
                    }
                }
            }
            Repeater {
                model: ["History", "Masks", "Filmstrip"]
                delegate: Rectangle {
                    Layout.preferredWidth: 160; Layout.fillHeight: true
                    radius: 6; color: "#20252c"; border.color: "#353c46"
                    Label { anchors.centerIn: parent; text: modelData
                        color: "#cbd5e1"; font.pixelSize: 12 }
                }
            }
            Label {
                text: documentController.aiStatus
                color: "#f59e0b"; font.pixelSize: 11
                visible: documentController.aiStatus.length > 0
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
        }
    }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.preferredWidth: 72; Layout.fillHeight: true
            color: "#181b20"; border.color: "#272d35"
            ColumnLayout {
                anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 14; spacing: 6
                Repeater {
                    model: [
                        { icon: "M", tip: "Move",         tool: 0 },
                        { icon: "C", tip: "Crop",         tool: 0 },
                        { icon: "B", tip: "Brush mask",   tool: 1 },
                        { icon: "E", tip: "Erase mask",   tool: 2 },
                        { icon: "G", tip: "Gradient mask",tool: 0 },
                        { icon: "R", tip: "Radial mask",  tool: 0 }
                    ]
                    delegate: Button {
                        Layout.preferredWidth: 44; Layout.preferredHeight: 38
                        text: modelData.icon
                        checkable: modelData.tool > 0
                        checked: modelData.tool > 0 && documentController.activeTool === modelData.tool
                        ToolTip.visible: hovered; ToolTip.text: modelData.tip
                        onClicked: {
                            if (modelData.tool > 0)
                                documentController.activeTool =
                                    (documentController.activeTool === modelData.tool) ? 0 : modelData.tool
                        }
                    }
                }
                Rectangle { width: 44; height: 1; color: "#2b313a" }
                Label { text: "Size"; color: "#6b7280"; font.pixelSize: 9
                    Layout.alignment: Qt.AlignHCenter }
                Slider {
                    from: 5; to: 200; value: root.brushRadius
                    orientation: Qt.Vertical; implicitHeight: 80
                    Layout.alignment: Qt.AlignHCenter
                    onMoved: root.brushRadius = value
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#101215"
            Flickable {
                id: canvasFlick; anchors.fill: parent
                contentWidth:  Math.max(width,  imagePreview.width)
                contentHeight: Math.max(height, imagePreview.height)
                clip: true
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const d = event.angleDelta.y > 0 ? 0.1 : -0.1
                        root.zoom = Math.min(4.0, Math.max(0.1, root.zoom + d))
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.max(360, imagePreview.width  + 72)
                    height: Math.max(260, imagePreview.height + 72)
                    color: "#0c0e11"; border.color: "#2d333c"
                    Image {
                        id: imagePreview; anchors.centerIn: parent
                        source: documentController.imageUrl
                        cache: false; fillMode: Image.PreserveAspectFit
                        width:  sourceSize.width  > 0 ? sourceSize.width  * root.zoom : 0
                        height: sourceSize.height > 0 ? sourceSize.height * root.zoom : 0
                        asynchronous: true
                        onSourceSizeChanged: root.zoom = root.fitZoom()
                    }
                    MaskCanvas {
                        id: maskOverlay
                        anchors.centerIn: parent
                        width:  imagePreview.width
                        height: imagePreview.height
                        visible: documentController.activeTool > 0
                        docCtrl:      documentController
                        brushRadius:  root.brushRadius
                        eraseMode:    documentController.activeTool === 2
                        paintEnabled: documentController.activeTool > 0
                    }
                    BusyIndicator {
                        anchors.centerIn: parent
                        visible: documentController.aiBusy
                        running: documentController.aiBusy
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !documentController.hasDocument
                        text: "Open an image to begin"
                        color: "#d0d5dd"; font.pixelSize: 22
                    }
                }
            }
            Row {
                anchors.left: parent.left; anchors.bottom: parent.bottom
                anchors.margins: 18; spacing: 8
                Button { text: "Fit";   enabled: documentController.hasDocument
                    onClicked: root.zoom = root.fitZoom() }
                Button { text: "100%";  enabled: documentController.hasDocument
                    onClicked: root.zoom = 1.0 }
                Button { text: "-";     enabled: documentController.hasDocument
                    onClicked: root.zoom = Math.max(0.1, root.zoom-0.1) }
                Button { text: "+";     enabled: documentController.hasDocument
                    onClicked: root.zoom = Math.min(4.0, root.zoom+0.1) }
                Button {
                    text: documentController.showOriginal ? "After" : "Before / After"
                    enabled: documentController.hasDocument
                    onClicked: documentController.showOriginal = !documentController.showOriginal
                }
                Button {
                    text: "Clear mask"
                    enabled: documentController.hasDocument && documentController.hasMask
                    onClicked: documentController.clearMask()
                }
            }
        }
        Rectangle {
            Layout.preferredWidth: 340; Layout.fillHeight: true
            color: "#1a1e24"; border.color: "#2c333d"
            ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width; spacing: 14; anchors.margins: 18
                    Label { text: "Adjustments"; color: "#f2f4f7"
                        font.pixelSize: 18; font.bold: true
                        Layout.leftMargin: 18; Layout.topMargin: 18 }
                    Label { text: "Transform"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true; Layout.leftMargin: 18 }
                    GridLayout {
                        Layout.leftMargin: 18; Layout.rightMargin: 18
                        Layout.fillWidth: true; columns: 2; rowSpacing: 6; columnSpacing: 6
                        Button { text: "Rotate left";  enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.rotateCounterClockwise() }
                        Button { text: "Rotate right"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.rotateClockwise() }
                        Button { text: "Flip H"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.flipHorizontal() }
                        Button { text: "Flip V"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.flipVertical() }
                    }
                    RowLayout {
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true; spacing: 6
                        Button { text: "Undo"; enabled: documentController.canUndo
                            Layout.fillWidth: true; onClicked: documentController.undo() }
                        Button { text: "Redo"; enabled: documentController.canRedo
                            Layout.fillWidth: true; onClicked: documentController.redo() }
                    }
                    AdjustmentSlider { label: "Exposure";    from: -3;   to: 3
                        value: documentController.exposure;    onMoved: (v) => documentController.exposure = v }
                    AdjustmentSlider { label: "Contrast";    from: -100; to: 100
                        value: documentController.contrast;    onMoved: (v) => documentController.contrast = v }
                    AdjustmentSlider { label: "Saturation";  from: -100; to: 100
                        value: documentController.saturation;  onMoved: (v) => documentController.saturation = v }
                    AdjustmentSlider { label: "Highlights";  from: -100; to: 100
                        value: documentController.highlights;  onMoved: (v) => documentController.highlights = v }
                    AdjustmentSlider { label: "Shadows";     from: -100; to: 100
                        value: documentController.shadows;     onMoved: (v) => documentController.shadows = v }
                    AdjustmentSlider { label: "Whites";      from: -100; to: 100
                        value: documentController.whites;      onMoved: (v) => documentController.whites = v }
                    AdjustmentSlider { label: "Blacks";      from: -100; to: 100
                        value: documentController.blacks;      onMoved: (v) => documentController.blacks = v }
                    AdjustmentSlider { label: "Vibrance";    from: -100; to: 100
                        value: documentController.vibrance;    onMoved: (v) => documentController.vibrance = v }
                    AdjustmentSlider { label: "Temperature"; from: -100; to: 100
                        value: documentController.temperature; onMoved: (v) => documentController.temperature = v }
                    AdjustmentSlider { label: "Tint";        from: -100; to: 100
                        value: documentController.tint;        onMoved: (v) => documentController.tint = v }
                    Label { text: "Detail"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true; Layout.leftMargin: 18 }
                    AdjustmentSlider { label: "Noise Reduction"; from: 0; to: 100
                        value: documentController.noiseReduction
                        onMoved: (v) => documentController.noiseReduction = v }
                    AdjustmentSlider { label: "Sharpening"; from: 0; to: 100
                        value: documentController.sharpening
                        onMoved: (v) => documentController.sharpening = v }
                    Button {
                        text: "Reset all"
                        enabled: documentController.hasDocument
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.resetAdjustments()
                    }
                    Label { text: "AI tools"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true
                        Layout.leftMargin: 18; Layout.topMargin: 8 }
                    Button {
                        text: "Subject mask"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.requestAiMask(
                            imagePreview.width / 2, imagePreview.height / 2)
                    }
                    Button {
                        text: "Background mask"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: {
                            documentController.requestAiMask(
                                imagePreview.width / 2, imagePreview.height / 2)
                        }
                    }
                    Button {
                        text: "Object removal"
                        enabled: documentController.hasDocument && documentController.hasMask
                               && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.applyInpaint()
                    }
                    Button {
                        text: "Upscale ×4"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.applyUpscale()
                    }
                    Item { Layout.preferredHeight: 24 }
                }
            }
        }
    }
    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real   from:  0
        property real   to:    1
        property real   value: 0
        signal moved(real nextValue)
        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true; spacing: 4
        RowLayout {
            Layout.fillWidth: true
            Label { text: sliderRoot.label; color: "#d0d5dd"; Layout.fillWidth: true }
            Label { text: Number(slider.value).toFixed(sliderRoot.to <= 3 ? 2 : 0)
                color: "#98a2b3" }
        }
        Slider {
            id: slider; Layout.fillWidth: true
            from: sliderRoot.from; to: sliderRoot.to; value: sliderRoot.value
            enabled: documentController.hasDocument
            onMoved: sliderRoot.moved(value)
        }
    }
}
