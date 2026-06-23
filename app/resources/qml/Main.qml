import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
ApplicationWindow {
    id: root
    width: 1440; height: 920
    visible: true
    title: "LumenForge"
    color: "#0f1117"
    property real zoom: 1.0
    property real brushRadius: 50
    function fitZoom() {
        if (imagePreview.sourceSize.width <= 0) return 1.0
        return Math.min(canvasFlick.width  / imagePreview.sourceSize.width,
                        canvasFlick.height / imagePreview.sourceSize.height) * 0.95
    }
    FileDialog { id: openImageDialog; title: "Open image"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp *.cr2 *.cr3 *.nef *.arw *.dng *.raf *.orf *.rw2)", "All files (*)"]
        onAccepted: documentController.openImage(selectedFile) }
    FileDialog { id: openProjectDialog; title: "Open project"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.loadProject(selectedFile) }
    FileDialog { id: saveProjectDialog; title: "Save project"
        fileMode: FileDialog.SaveFile; defaultSuffix: "lfproj"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.saveProject(selectedFile) }
    FileDialog { id: exportDialog; title: "Export image"
        fileMode: FileDialog.SaveFile; defaultSuffix: "png"
        nameFilters: ["PNG (*.png)","JPEG (*.jpg)","WebP (*.webp)"]
        onAccepted: documentController.exportImage(selectedFile) }
    FileDialog { id: addLayerDialog; title: "Add image layer"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp)"]
        onAccepted: documentController.addImageLayer(selectedFile) }
    Dialog { id: recoveryDialog; title: "Recover unsaved work?"; modal: true
        visible: documentController.hasPendingRecovery; anchors.centerIn: parent
        Label { text: "An autosaved project was found. Recover it?"; color: "#e2e8f0" }
        footer: DialogButtonBox {
            Button { text: "Recover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { documentController.recoverProject(); recoveryDialog.close() } }
            Button { text: "Discard"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { documentController.discardRecovery(); recoveryDialog.close() } }
        }
    }
    // Shortcuts
    Shortcut { sequence: StandardKey.Open;  onActivated: openImageDialog.open() }
    Shortcut { sequence: StandardKey.Save;  onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+E";          onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+0";          onActivated: root.zoom = 1.0 }
    Shortcut { sequence: "Ctrl++";          onActivated: root.zoom = Math.min(4.0, root.zoom + 0.1) }
    Shortcut { sequence: "Ctrl+-";          onActivated: root.zoom = Math.max(0.1, root.zoom - 0.1) }
    Shortcut { sequence: StandardKey.Undo;  onActivated: documentController.undo() }
    Shortcut { sequence: StandardKey.Redo;  onActivated: documentController.redo() }
    Shortcut { sequence: "\\\\";            onActivated: documentController.showOriginal = !documentController.showOriginal }
    Shortcut { sequence: "Escape";          onActivated: documentController.activeTool = 0 }
    Shortcut { sequence: "B"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool === 1 ? 0 : 1) }
    Shortcut { sequence: "E"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool === 2 ? 0 : 2) }
    Shortcut { sequence: "G"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool === 3 ? 0 : 3) }
    Shortcut { sequence: "R"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool === 4 ? 0 : 4) }
    Shortcut { sequence: "C"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool === 5 ? 0 : 5) }
    // Header
    header: ToolBar {
        height: 52
        background: Rectangle { color: "#13161f"; border.color: "#1e2438"; border.width: 1 }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 8
            // Logo mark
            Rectangle { width: 8; height: 28; radius: 4
                gradient: Gradient { GradientStop { position: 0; color: "#6366f1" } GradientStop { position: 1; color: "#818cf8" } } }
            Label { text: "LumenForge"; color: "#e2e8f0"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Label { text: documentController.sourceName; color: "#6b7a99"
                elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 12 }
            Label { text: documentController.aiStatus; color: "#f59e0b"; font.pixelSize: 11
                visible: documentController.aiStatus.length > 0 }
            Button { text: "Open";    onClicked: openImageDialog.open()
                background: Rectangle { color: parent.hovered ? "#1e2438" : "#161a28"; radius: 7; border.color: "#252d45" }
                contentItem: Label { text: "Open";    color: "#c8d0e0"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter } }
            Button { text: "Project"; onClicked: openProjectDialog.open()
                background: Rectangle { color: parent.hovered ? "#1e2438" : "#161a28"; radius: 7; border.color: "#252d45" }
                contentItem: Label { text: "Project"; color: "#c8d0e0"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter } }
            Button { text: "Save";    enabled: documentController.hasDocument; onClicked: saveProjectDialog.open()
                background: Rectangle { color: parent.hovered ? "#1e2438" : "#161a28"; radius: 7; border.color: "#252d45" }
                contentItem: Label { text: "Save";    color: parent.enabled ? "#c8d0e0" : "#4a5268"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter } }
            Button { text: "Export";  enabled: documentController.hasDocument; onClicked: exportDialog.open()
                background: Rectangle { color: parent.hovered ? "#252d6a" : "#1c2058"; radius: 7; border.color: "#3d41a0" }
                contentItem: Label { text: "Export";  color: parent.enabled ? "#c7d2fe" : "#4a5268"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter } }
        }
    }
    // Footer
    footer: Rectangle {
        height: 118; color: "#13161f"; border.color: "#1e2438"
        RowLayout {
            anchors.fill: parent; anchors.margins: 8; spacing: 8
            Rectangle {
                Layout.preferredWidth: 310; Layout.fillHeight: true
                color: "#171c2a"; radius: 8; border.color: "#1e2438"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 4
                    RowLayout {
                        Label { text: "Layers"; color: "#c8d0e0"; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        Button { text: "+"; implicitWidth: 26; implicitHeight: 22; flat: true
                            enabled: documentController.hasDocument; onClicked: addLayerDialog.open()
                            contentItem: Label { text: "+"; color: "#6366f1"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter } }
                    }
                    ListView { id: layerList; Layout.fillWidth: true; Layout.fillHeight: true
                        model: documentController.layerModel; clip: true
                        delegate: Rectangle { width: layerList.width; height: 26; color: "transparent"
                            RowLayout { anchors.fill: parent; anchors.margins: 2; spacing: 4
                                Button { text: modelData.visible ? "◉" : "○"; flat: true; implicitWidth: 22; implicitHeight: 22
                                    onClicked: documentController.setLayerVisible(modelData.id, !modelData.visible)
                                    contentItem: Label { text: parent.text; color: modelData.visible ? "#6366f1" : "#4a5268"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                                Label { text: modelData.name; color: "#c8d0e0"; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                                Slider { from: 0; to: 1; value: modelData.opacity; implicitWidth: 60; implicitHeight: 20
                                    onMoved: documentController.setLayerOpacity(modelData.id, value) }
                                Button { text: "✕"; flat: true; implicitWidth: 22; implicitHeight: 22
                                    onClicked: documentController.deleteLayer(modelData.id)
                                    contentItem: Label { text: "✕"; color: "#f07070"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter } }
                            }
                        }
                    }
                }
            }
            Repeater {
                model: ["History", "Masks", "Filmstrip"]
                delegate: Rectangle { Layout.preferredWidth: 148; Layout.fillHeight: true
                    color: "#171c2a"; radius: 8; border.color: "#1e2438"
                    Label { anchors.centerIn: parent; text: modelData; color: "#4a5268"; font.pixelSize: 11 } }
            }
            Item { Layout.fillWidth: true }
        }
    }
    // Main 3-column layout
    RowLayout { anchors.fill: parent; spacing: 0
        // ── Tool rail ────────────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 68; Layout.fillHeight: true
            color: "#13161f"; border.color: "#1e2438"
            ColumnLayout {
                anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 14; spacing: 5
                // Tools: 0=navigate, 1=brush, 2=erase, 3=gradient, 4=radial, 5=crop
                Repeater {
                    model: [
                        { icon: "✥",  tip: "Navigate / Pan\nRight-click also pans",  tool: 0 },
                        { icon: "⬤",  tip: "Brush Mask  [B]",                         tool: 1 },
                        { icon: "◯",  tip: "Erase Mask  [E]",                         tool: 2 },
                        { icon: "▬",  tip: "Gradient Mask  [G]\nDrag start → end",    tool: 3 },
                        { icon: "◎",  tip: "Radial Mask  [R]\nDrag center → edge",    tool: 4 },
                        { icon: "⊡",  tip: "Crop  [C]\nDrag to select area",          tool: 5 },
                    ]
                    delegate: Button {
                        Layout.preferredWidth: 46; Layout.preferredHeight: 40
                        enabled: modelData.tool === 0 || documentController.hasDocument
                        checkable: modelData.tool > 0
                        checked: modelData.tool > 0 && documentController.activeTool === modelData.tool
                        ToolTip.visible: hovered; ToolTip.text: modelData.tip; ToolTip.delay: 500
                        onClicked: {
                            if (modelData.tool === 0) documentController.activeTool = 0
                            else documentController.activeTool =
                                (documentController.activeTool === modelData.tool) ? 0 : modelData.tool
                        }
                        background: Rectangle {
                            radius: 8
                            color: parent.checked ? "#4f46e5"
                                 : parent.hovered  ? "#1e2438"
                                 : "transparent"
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                        contentItem: Label {
                            text: modelData.icon
                            color: parent.checked ? "#ffffff"
                                 : parent.enabled  ? "#8892a4"
                                 : "#2a3050"
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 15
                        }
                    }
                }
                Rectangle { width: 42; height: 1; color: "#1e2438"; Layout.alignment: Qt.AlignHCenter }
                Label { text: "SIZE"; color: "#3a4566"; font.pixelSize: 8; Layout.alignment: Qt.AlignHCenter; letterSpacing: 1 }
                Slider {
                    from: 5; to: 200; value: root.brushRadius
                    orientation: Qt.Vertical; implicitHeight: 88; Layout.alignment: Qt.AlignHCenter
                    visible: documentController.activeTool === 1 || documentController.activeTool === 2
                    onMoved: root.brushRadius = value
                }
            }
        }
        // ── Canvas ───────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#0a0c12"
            Flickable {
                id: canvasFlick; anchors.fill: parent
                contentWidth:  Math.max(width,  imagePreview.width  + 80)
                contentHeight: Math.max(height, imagePreview.height + 80)
                clip: true
                // Only left-drag-to-pan when no tool is active;
                // right-click panning is handled by the MouseArea below
                interactive: documentController.activeTool === 0
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const d = event.angleDelta.y > 0 ? 0.1 : -0.1
                        root.zoom = Math.min(4.0, Math.max(0.1, root.zoom + d))
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.max(380, imagePreview.width  + 80)
                    height: Math.max(280, imagePreview.height + 80)
                    color: "#08090e"; border.color: "#1a1e2e"; radius: 3
                    Image {
                        id: imagePreview; anchors.centerIn: parent
                        source: documentController.imageUrl
                        cache: false; fillMode: Image.PreserveAspectFit
                        width:  sourceSize.width  > 0 ? sourceSize.width  * root.zoom : 0
                        height: sourceSize.height > 0 ? sourceSize.height * root.zoom : 0
                        asynchronous: true; smooth: true
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
                    BusyIndicator { anchors.centerIn: parent
                        visible: documentController.aiBusy; running: documentController.aiBusy }
                    Label { anchors.centerIn: parent
                        visible: !documentController.hasDocument
                        text: "Open an image to begin"
                        color: "#3a4566"; font.pixelSize: 20 }
                }
                // ── Right / middle-click pan ──────────────────────────────
                // Sits ABOVE MaskCanvas in z-order; only grabs right+middle so
                // left-click still falls through to MaskCanvas for painting.
                MouseArea {
                    id: panArea; anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.MiddleButton
                    propagateComposedEvents: true
                    property real lx: 0; property real ly: 0
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
                    onPressed: (mouse) => { lx = mouse.x; ly = mouse.y; mouse.accepted = true }
                    onPositionChanged: (mouse) => {
                        const dx = mouse.x - lx; const dy = mouse.y - ly
                        canvasFlick.contentX = Math.max(0, Math.min(canvasFlick.contentWidth  - canvasFlick.width,  canvasFlick.contentX - dx))
                        canvasFlick.contentY = Math.max(0, Math.min(canvasFlick.contentHeight - canvasFlick.height, canvasFlick.contentY - dy))
                        lx = mouse.x; ly = mouse.y
                    }
                }
            }
            // Canvas bottom bar
            Row {
                anchors.left: parent.left; anchors.bottom: parent.bottom
                anchors.margins: 14; spacing: 6
                Button { text: "Fit";  enabled: documentController.hasDocument; implicitHeight: 28; implicitWidth: 40
                    onClicked: root.zoom = root.fitZoom()
                    background: Rectangle { color: parent.hovered ? "#1e2438" : "#0f1219"; radius: 6; border.color: "#1e2438" }
                    contentItem: Label { text: "Fit";  color: "#6b7a99"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                Button { text: "100%"; enabled: documentController.hasDocument; implicitHeight: 28; implicitWidth: 44
                    onClicked: root.zoom = 1.0
                    background: Rectangle { color: parent.hovered ? "#1e2438" : "#0f1219"; radius: 6; border.color: "#1e2438" }
                    contentItem: Label { text: "100%"; color: "#6b7a99"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                Button { text: "−"; enabled: documentController.hasDocument; implicitHeight: 28; implicitWidth: 30
                    onClicked: root.zoom = Math.max(0.1, root.zoom - 0.1)
                    background: Rectangle { color: parent.hovered ? "#1e2438" : "#0f1219"; radius: 6; border.color: "#1e2438" }
                    contentItem: Label { text: "−"; color: "#6b7a99"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter } }
                Button { text: "+"; enabled: documentController.hasDocument; implicitHeight: 28; implicitWidth: 30
                    onClicked: root.zoom = Math.min(4.0, root.zoom + 0.1)
                    background: Rectangle { color: parent.hovered ? "#1e2438" : "#0f1219"; radius: 6; border.color: "#1e2438" }
                    contentItem: Label { text: "+"; color: "#6b7a99"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter } }
                Button {
                    text: documentController.showOriginal ? "After" : "Before / After"
                    enabled: documentController.hasDocument; implicitHeight: 28
                    onClicked: documentController.showOriginal = !documentController.showOriginal
                    background: Rectangle { color: parent.hovered ? "#1e2438" : "#0f1219"; radius: 6; border.color: "#1e2438" }
                    contentItem: Label { text: parent.text; color: "#6b7a99"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                Button {
                    text: "Clear mask"
                    enabled: documentController.hasDocument && documentController.hasMask; implicitHeight: 28
                    onClicked: documentController.clearMask()
                    background: Rectangle { color: parent.hovered ? "#2a1414" : "#0f1219"; radius: 6; border.color: "#1e2438" }
                    contentItem: Label { text: "Clear mask"; color: "#f07070"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
            }
            // Zoom %
            Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
                width: 52; height: 24; radius: 5; color: "#0f1219"; border.color: "#1e2438"
                Label { anchors.centerIn: parent; text: Math.round(root.zoom * 100) + "%"
                    color: "#6b7a99"; font.pixelSize: 11 } }
        }
        // ── Adjustments panel ─────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 310; Layout.fillHeight: true
            color: "#13161f"; border.color: "#1e2438"
            ScrollView { anchors.fill: parent
                ColumnLayout {
                    width: 310; spacing: 12
                    // Header
                    Item { Layout.fillWidth: true; Layout.preferredHeight: 52
                        Label { anchors.left: parent.left; anchors.leftMargin: 18
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Adjustments"; color: "#e2e8f0"; font.pixelSize: 17; font.weight: Font.DemiBold } }
                    // Transform
                    Label { text: "TRANSFORM"; color: "#3a4566"; font.pixelSize: 10; font.weight: Font.Medium
                        Layout.leftMargin: 18; letterSpacing: 1.2 }
                    GridLayout { Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true
                        columns: 2; rowSpacing: 6; columnSpacing: 6
                        Button { text: "↺ Left";   Layout.fillWidth: true; implicitHeight: 30; enabled: documentController.hasDocument; onClicked: documentController.rotateCounterClockwise()
                            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 7; border.color: "#252d45" }
                            contentItem: Label { text: parent.text; color: "#8892a4"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                        Button { text: "↻ Right";  Layout.fillWidth: true; implicitHeight: 30; enabled: documentController.hasDocument; onClicked: documentController.rotateClockwise()
                            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 7; border.color: "#252d45" }
                            contentItem: Label { text: parent.text; color: "#8892a4"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                        Button { text: "⇔ Flip H"; Layout.fillWidth: true; implicitHeight: 30; enabled: documentController.hasDocument; onClicked: documentController.flipHorizontal()
                            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 7; border.color: "#252d45" }
                            contentItem: Label { text: parent.text; color: "#8892a4"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                        Button { text: "⇕ Flip V"; Layout.fillWidth: true; implicitHeight: 30; enabled: documentController.hasDocument; onClicked: documentController.flipVertical()
                            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 7; border.color: "#252d45" }
                            contentItem: Label { text: parent.text; color: "#8892a4"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                    }
                    RowLayout { Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true; spacing: 6
                        Button { Layout.fillWidth: true; text: "Undo"; enabled: documentController.canUndo; implicitHeight: 30; onClicked: documentController.undo()
                            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 7; border.color: "#252d45" }
                            contentItem: Label { text: parent.text; color: "#8892a4"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                        Button { Layout.fillWidth: true; text: "Redo"; enabled: documentController.canRedo; implicitHeight: 30; onClicked: documentController.redo()
                            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 7; border.color: "#252d45" }
                            contentItem: Label { text: parent.text; color: "#8892a4"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                    }
                    // Light
                    Label { text: "LIGHT"; color: "#3a4566"; font.pixelSize: 10; font.weight: Font.Medium; Layout.leftMargin: 18; letterSpacing: 1.2 }
                    AdjustmentSlider { label: "Exposure";   from: -3;   to: 3;   value: documentController.exposure;    onMoved: (v) => documentController.exposure    = v }
                    AdjustmentSlider { label: "Contrast";   from: -100; to: 100; value: documentController.contrast;    onMoved: (v) => documentController.contrast    = v }
                    AdjustmentSlider { label: "Highlights"; from: -100; to: 100; value: documentController.highlights;  onMoved: (v) => documentController.highlights  = v }
                    AdjustmentSlider { label: "Shadows";    from: -100; to: 100; value: documentController.shadows;     onMoved: (v) => documentController.shadows     = v }
                    AdjustmentSlider { label: "Whites";     from: -100; to: 100; value: documentController.whites;      onMoved: (v) => documentController.whites      = v }
                    AdjustmentSlider { label: "Blacks";     from: -100; to: 100; value: documentController.blacks;      onMoved: (v) => documentController.blacks      = v }
                    // Color
                    Label { text: "COLOR"; color: "#3a4566"; font.pixelSize: 10; font.weight: Font.Medium; Layout.leftMargin: 18; letterSpacing: 1.2 }
                    AdjustmentSlider { label: "Saturation";  from: -100; to: 100; value: documentController.saturation;  onMoved: (v) => documentController.saturation  = v }
                    AdjustmentSlider { label: "Vibrance";    from: -100; to: 100; value: documentController.vibrance;    onMoved: (v) => documentController.vibrance    = v }
                    AdjustmentSlider { label: "Temperature"; from: -100; to: 100; value: documentController.temperature; onMoved: (v) => documentController.temperature = v }
                    AdjustmentSlider { label: "Tint";        from: -100; to: 100; value: documentController.tint;        onMoved: (v) => documentController.tint        = v }
                    // Detail
                    Label { text: "DETAIL"; color: "#3a4566"; font.pixelSize: 10; font.weight: Font.Medium; Layout.leftMargin: 18; letterSpacing: 1.2 }
                    AdjustmentSlider { label: "Noise Reduction"; from: 0; to: 100; value: documentController.noiseReduction; onMoved: (v) => documentController.noiseReduction = v }
                    AdjustmentSlider { label: "Sharpening";      from: 0; to: 100; value: documentController.sharpening;     onMoved: (v) => documentController.sharpening     = v }
                    Button { text: "Reset All"; enabled: documentController.hasDocument; implicitHeight: 30
                        Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true
                        onClicked: documentController.resetAdjustments()
                        background: Rectangle { color: parent.hovered ? "#2a1414" : "#171c2a"; radius: 7; border.color: "#252d45" }
                        contentItem: Label { text: "Reset All"; color: "#f07070"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                    // AI
                    Label { text: "AI TOOLS"; color: "#3a4566"; font.pixelSize: 10; font.weight: Font.Medium; Layout.leftMargin: 18; letterSpacing: 1.2 }
                    Button { text: "Subject mask"; enabled: documentController.hasDocument && !documentController.aiBusy; implicitHeight: 30
                        Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true
                        onClicked: documentController.requestAiMask(imagePreview.width/2, imagePreview.height/2)
                        background: Rectangle { color: parent.hovered ? "#252d6a" : "#1c2058"; radius: 7; border.color: "#3d41a0" }
                        contentItem: Label { text: parent.text; color: parent.enabled ? "#c7d2fe" : "#4a5268"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                    Button { text: "Object removal"; enabled: documentController.hasDocument && documentController.hasMask && !documentController.aiBusy; implicitHeight: 30
                        Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true
                        onClicked: documentController.applyInpaint()
                        background: Rectangle { color: parent.hovered ? "#252d6a" : "#1c2058"; radius: 7; border.color: "#3d41a0" }
                        contentItem: Label { text: parent.text; color: parent.enabled ? "#c7d2fe" : "#4a5268"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                    Button { text: "Upscale x4"; enabled: documentController.hasDocument && !documentController.aiBusy; implicitHeight: 30
                        Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true
                        onClicked: documentController.applyUpscale()
                        background: Rectangle { color: parent.hovered ? "#252d6a" : "#1c2058"; radius: 7; border.color: "#3d41a0" }
                        contentItem: Label { text: parent.text; color: parent.enabled ? "#c7d2fe" : "#4a5268"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter } }
                    Item { Layout.preferredHeight: 20 }
                }
            }
        }
    }
    // AdjustmentSlider component — uses explicit sliderRoot id to avoid scope issues
    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real   from:  0
        property real   to:    1
        property real   value: 0
        signal moved(real nextValue)
        Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true; spacing: 3
        RowLayout { Layout.fillWidth: true
            Label { text: sliderRoot.label; color: "#8892a4"; font.pixelSize: 11; Layout.fillWidth: true }
            Label { text: Number(sl.value).toFixed(sliderRoot.to <= 3 ? 2 : 0)
                color: sl.value !== 0 ? "#6366f1" : "#4a5268"; font.pixelSize: 11 }
        }
        Slider { id: sl; Layout.fillWidth: true; implicitHeight: 20
            from: sliderRoot.from; to: sliderRoot.to; value: sliderRoot.value
            enabled: documentController.hasDocument
            onMoved: sliderRoot.moved(value)
            background: Rectangle {
                x: sl.leftPadding; y: sl.topPadding + sl.availableHeight/2 - height/2
                width: sl.availableWidth; height: 3; radius: 1.5; color: "#1a1e2e"
                Rectangle { width: sl.visualPosition * parent.width; height: parent.height; radius: 1.5
                    color: sl.value !== 0 ? "#6366f1" : "#252d45" }
            }
            handle: Rectangle {
                x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
                y: sl.topPadding + sl.availableHeight/2 - height/2
                width: 14; height: 14; radius: 7
                color: sl.pressed ? "#818cf8" : (sl.hovered ? "#818cf8" : "#6366f1")
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
    }
}