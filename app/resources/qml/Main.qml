import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.settings 1.0

ApplicationWindow {
    id: root
    width: 1440; height: 920
    visible: true
    // File name shown in OS title bar — no wasted space in the canvas area
    title: documentController.hasDocument
        ? "LumenForge — " + documentController.sourceName
        : "LumenForge"
    color: "#0f1117"

    property real   zoom:            1.0
    property real   brushRadius:     50
    property string lastSourceName:  ""
    property int    bottomTab:       0    // 0=Layers 1=History 2=Masks 3=Filmstrip

    Settings {
        id: ui
        category: "MainWindow"
        property real bottomHeight: 160
    }

    function fitZoom() {
        if (imagePreview.sourceSize.width <= 0) return 1.0
        return Math.min(canvasFlick.width  / imagePreview.sourceSize.width,
                        canvasFlick.height / imagePreview.sourceSize.height) * 0.95
    }

    // Only reset zoom when a GENUINELY NEW document is opened —
    // not when the preview JPEG refreshes (which also changes sourceSize).
    Connections {
        target: documentController
        function onDocumentChanged() {
            const n = documentController.sourceName;
            if (n !== root.lastSourceName && n !== "No image loaded") {
                root.lastSourceName = n;
                Qt.callLater(function() { root.zoom = root.fitZoom(); });
            }
        }
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────
    FileDialog { id: openImageDialog; title: "Open image"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp *.cr2 *.cr3 *.nef *.arw *.dng *.raf *.orf *.rw2)","All files (*)"]
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
        visible: documentController.hasPendingRecovery
        anchors.centerIn: parent
        Label { text: "An autosaved project was found. Recover it?"; color: "#e2e8f0" }
        footer: DialogButtonBox {
            Button { text: "Recover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { documentController.recoverProject(); recoveryDialog.close() } }
            Button { text: "Discard"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { documentController.discardRecovery(); recoveryDialog.close() } }
        }
    }

    // ── Shortcuts ─────────────────────────────────────────────────────────────
    Shortcut { sequence: StandardKey.Open;  onActivated: openImageDialog.open() }
    Shortcut { sequence: StandardKey.Save;  onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+E";          onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+0";          onActivated: root.zoom = 1.0 }
    Shortcut { sequence: "Ctrl++";          onActivated: root.zoom = Math.min(4.0, root.zoom * 1.12) }
    Shortcut { sequence: "Ctrl+-";          onActivated: root.zoom = Math.max(0.1, root.zoom / 1.12) }
    Shortcut { sequence: StandardKey.Undo;  onActivated: documentController.undo() }
    Shortcut { sequence: StandardKey.Redo;  onActivated: documentController.redo() }
    Shortcut { sequence: "\\\\";            onActivated: documentController.showOriginal = !documentController.showOriginal }
    Shortcut { sequence: "Escape";          onActivated: documentController.activeTool = 0 }
    Shortcut { sequence: "B"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===1?0:1) }
    Shortcut { sequence: "E"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===2?0:2) }
    Shortcut { sequence: "G"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===3?0:3) }
    Shortcut { sequence: "R"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===4?0:4) }
    Shortcut { sequence: "C"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===5?0:5) }
    Shortcut { sequence: "Return"
        enabled: documentController.activeTool===5 && documentController.hasDocument
        onActivated: cropOverlayItem.confirm() }

    // ── Root layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Compact toolbar (replaces header: ToolBar to reclaim vertical space)
        Rectangle {
            Layout.fillWidth: true
            height: 42
            color: "#13161f"
            // Bottom border only
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#1e2438" }
            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                spacing: 4
                // Logo mark
                Rectangle { width: 6; height: 24; radius: 3
                    gradient: Gradient { GradientStop{position:0;color:"#6366f1"} GradientStop{position:1;color:"#818cf8"} } }
                Item { width: 6 }
                Button { text: "Open"
                    background: Rectangle { color: parent.hovered?"#1e2438":"#161a28"; radius:6; border.color:"#252d45" }
                    contentItem: Label { text:"Open"; color:"#c8d0e0"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 54
                    onClicked: openImageDialog.open() }
                Button { text: "Project"
                    background: Rectangle { color: parent.hovered?"#1e2438":"#161a28"; radius:6; border.color:"#252d45" }
                    contentItem: Label { text:"Project"; color:"#c8d0e0"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 60
                    onClicked: openProjectDialog.open() }
                Button { text: "Save"; enabled: documentController.hasDocument
                    background: Rectangle { color: parent.hovered?"#1e2438":"#161a28"; radius:6; border.color:"#252d45" }
                    contentItem: Label { text:"Save"; color:parent.enabled?"#c8d0e0":"#4a5268"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 50
                    onClicked: saveProjectDialog.open() }
                Button { text: "Export"; enabled: documentController.hasDocument
                    background: Rectangle { color: parent.hovered?"#252d6a":"#1c2058"; radius:6; border.color:"#3d41a0" }
                    contentItem: Label { text:"Export"; color:parent.enabled?"#c7d2fe":"#4a5268"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 58
                    onClicked: exportDialog.open() }
                Item { Layout.fillWidth: true }
                Label { text: documentController.aiStatus; color:"#f59e0b"; font.pixelSize:11
                    visible: documentController.aiStatus.length>0 }
                BusyIndicator { running: documentController.aiBusy; visible: documentController.aiBusy
                    implicitWidth: 22; implicitHeight: 22 }
            }
        }

        // ── Main SplitView (canvas top, panels bottom) ─────────────────────
        SplitView {
            id: mainSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            handle: Rectangle {
                implicitHeight: 5
                color: SplitHandle.pressed ? "#6366f1"
                     : SplitHandle.hovered ? "#2d3050" : "#13161f"
                Rectangle { width:36; height:2; radius:1; anchors.centerIn:parent; color:"#2a3050" }
            }

            Component.onCompleted: {
                if (ui.bottomHeight > 0)
                    bottomPanel.SplitView.preferredHeight = ui.bottomHeight;
            }
            onResizingChanged: {
                if (!resizing) ui.bottomHeight = bottomPanel.SplitView.preferredHeight;
            }

            // ── Top: tool rail + canvas + adjustments ─────────────────────
            Item {
                SplitView.fillHeight:   true
                SplitView.minimumHeight: 180
                RowLayout { anchors.fill: parent; spacing: 0

                    // Tool rail
                    Rectangle {
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        color: "#13161f"
                        Rectangle { anchors.right:parent.right; width:1; height:parent.height; color:"#1e2438" }
                        ColumnLayout {
                            anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
                            anchors.topMargin: 12; spacing: 4
                            Repeater {
                                model: [
                                    {icon:"✥",tip:"Navigate",          tool:0},
                                    {icon:"⬤",tip:"Brush Mask  [B]",   tool:1},
                                    {icon:"◯",tip:"Erase Mask  [E]",   tool:2},
                                    {icon:"▬",tip:"Gradient  [G]",     tool:3},
                                    {icon:"◎",tip:"Radial  [R]",       tool:4},
                                    {icon:"⊡",tip:"Crop  [C]",         tool:5},
                                ]
                                delegate: Button {
                                    Layout.preferredWidth:46; Layout.preferredHeight:38
                                    enabled: modelData.tool===0||documentController.hasDocument
                                    checkable: modelData.tool>0
                                    checked: modelData.tool>0&&documentController.activeTool===modelData.tool
                                    ToolTip.visible:hovered; ToolTip.text:modelData.tip; ToolTip.delay:500
                                    onClicked: {
                                        if (modelData.tool===0) documentController.activeTool=0
                                        else documentController.activeTool=(documentController.activeTool===modelData.tool)?0:modelData.tool
                                    }
                                    background: Rectangle { radius:7
                                        color: parent.checked?"#4f46e5":parent.hovered?"#1e2438":"transparent"
                                        Behavior on color { ColorAnimation{duration:120} }
                                    }
                                    contentItem: Label { text:modelData.icon; font.pixelSize:14
                                        color: parent.checked?"#fff":parent.enabled?"#8892a4":"#2a3050"
                                        horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
                                }
                            }
                            Rectangle { width:42; height:1; color:"#1e2438"; Layout.alignment:Qt.AlignHCenter }
                            Label { text:"SIZE"; color:"#3a4566"; font.pixelSize:8; Layout.alignment:Qt.AlignHCenter }
                            Slider {
                                from:5; to:200; value:root.brushRadius; orientation:Qt.Vertical
                                implicitHeight:80; Layout.alignment:Qt.AlignHCenter
                                visible: documentController.activeTool===1||documentController.activeTool===2
                                onMoved: root.brushRadius=value
                            }
                        }
                    }

                    // Canvas
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: "#0a0c12"
                        Flickable {
                            id: canvasFlick; anchors.fill: parent
                            contentWidth:  Math.max(width,  imagePreview.width  + 80)
                            contentHeight: Math.max(height, imagePreview.height + 80)
                            clip: true
                            interactive: documentController.activeTool===0
                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse|PointerDevice.TouchPad
                                onWheel: (event) => {
                                    const oldZ=root.zoom, fac=event.angleDelta.y>0?1.12:(1.0/1.12);
                                    const newZ=Math.min(4.0,Math.max(0.1,oldZ*fac));
                                    if (Math.abs(newZ-oldZ)<0.001) return;
                                    const imgW=imagePreview.sourceSize.width>0?imagePreview.sourceSize.width*oldZ:0;
                                    const imgH=imagePreview.sourceSize.height>0?imagePreview.sourceSize.height*oldZ:0;
                                    const cW=Math.max(canvasFlick.width,imgW+80), cH=Math.max(canvasFlick.height,imgH+80);
                                    const cx=canvasFlick.contentX+event.x, cy=canvasFlick.contentY+event.y;
                                    const relX=(cx-cW*0.5)/oldZ, relY=(cy-cH*0.5)/oldZ;
                                    root.zoom=newZ;
                                    const nW=Math.max(canvasFlick.width,imagePreview.sourceSize.width*newZ+80);
                                    const nH=Math.max(canvasFlick.height,imagePreview.sourceSize.height*newZ+80);
                                    canvasFlick.contentX=Math.max(0,Math.min(nW-canvasFlick.width, nW*0.5+relX*newZ-event.x));
                                    canvasFlick.contentY=Math.max(0,Math.min(nH-canvasFlick.height,nH*0.5+relY*newZ-event.y));
                                }
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width:  Math.max(380, imagePreview.width  + 80)
                                height: Math.max(280, imagePreview.height + 80)
                                color:"#08090e"; border.color:"#1a1e2e"; radius:3
                                Image {
                                    id: imagePreview; anchors.centerIn: parent
                                    source: documentController.imageUrl
                                    cache:false; fillMode:Image.PreserveAspectFit; asynchronous:true; smooth:true
                                    width:  sourceSize.width>0  ? sourceSize.width  * root.zoom : 0
                                    height: sourceSize.height>0 ? sourceSize.height * root.zoom : 0
                                    // NO onSourceSizeChanged here — zoom is managed by the
                                    // Connections block above (only resets on new document)
                                }
                                MaskCanvas {
                                    id: maskOverlay; anchors.centerIn: parent
                                    width: imagePreview.width; height: imagePreview.height
                                    visible: documentController.hasDocument &&
                                             documentController.activeTool>=1 &&
                                             documentController.activeTool<=4
                                    docCtrl:      documentController
                                    brushRadius:  root.brushRadius
                                    eraseMode:    documentController.activeTool===2
                                    paintEnabled: documentController.activeTool===1 ||
                                                  documentController.activeTool===2
                                }
                                CropOverlay {
                                    id: cropOverlayItem; anchors.centerIn: parent
                                    width: imagePreview.width; height: imagePreview.height
                                    visible: documentController.hasDocument &&
                                             documentController.activeTool===5
                                    docCtrl: documentController
                                }
                                Label { anchors.centerIn:parent
                                    visible:!documentController.hasDocument
                                    text:"Open an image to begin"; color:"#3a4566"; font.pixelSize:20 }
                            }
                            MouseArea {
                                anchors.fill:parent; acceptedButtons:Qt.RightButton|Qt.MiddleButton
                                propagateComposedEvents:true; property real lx:0; property real ly:0
                                cursorShape: pressed?Qt.ClosedHandCursor:Qt.ArrowCursor
                                onPressed: (m)=>{lx=m.x;ly=m.y;m.accepted=true}
                                onPositionChanged: (m)=>{
                                    canvasFlick.contentX=Math.max(0,Math.min(canvasFlick.contentWidth-canvasFlick.width,  canvasFlick.contentX-(m.x-lx)));
                                    canvasFlick.contentY=Math.max(0,Math.min(canvasFlick.contentHeight-canvasFlick.height,canvasFlick.contentY-(m.y-ly)));
                                    lx=m.x;ly=m.y;
                                }
                            }
                        }
                        // Canvas bottom bar
                        Row {
                            anchors.left:parent.left; anchors.bottom:parent.bottom; anchors.margins:12; spacing:5
                            Button { text:"Fit";   enabled:documentController.hasDocument; implicitHeight:26; implicitWidth:38
                                onClicked:root.zoom=root.fitZoom()
                                background:Rectangle{color:parent.hovered?"#1e2438":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:"Fit";color:"#6b7a99";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter} }
                            Button { text:"100%"; enabled:documentController.hasDocument; implicitHeight:26; implicitWidth:42
                                onClicked:root.zoom=1.0
                                background:Rectangle{color:parent.hovered?"#1e2438":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:"100%";color:"#6b7a99";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter} }
                            Button { text:"−"; enabled:documentController.hasDocument; implicitHeight:26; implicitWidth:26
                                onClicked:root.zoom=Math.max(0.1,root.zoom/1.12)
                                background:Rectangle{color:parent.hovered?"#1e2438":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:"−";color:"#6b7a99";font.pixelSize:14;horizontalAlignment:Text.AlignHCenter} }
                            Button { text:"+"; enabled:documentController.hasDocument; implicitHeight:26; implicitWidth:26
                                onClicked:root.zoom=Math.min(4.0,root.zoom*1.12)
                                background:Rectangle{color:parent.hovered?"#1e2438":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:"+";color:"#6b7a99";font.pixelSize:14;horizontalAlignment:Text.AlignHCenter} }
                            Button {
                                // Issue 11 fix: label describes what you ARE SEEING
                                // showOriginal=true  → "Before" (seeing original)
                                // showOriginal=false → "After"  (seeing edited)
                                text: documentController.showOriginal ? "Before" : "After"
                                enabled:documentController.hasDocument; implicitHeight:26
                                onClicked:documentController.showOriginal=!documentController.showOriginal
                                background:Rectangle{color:parent.hovered?"#1e2438":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:parent.text;color:"#6b7a99";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter} }
                            Button { text:"Clear mask"
                                enabled:documentController.hasDocument&&documentController.hasMask; implicitHeight:26
                                onClicked:documentController.clearMask()
                                background:Rectangle{color:parent.hovered?"#2a1414":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:"Clear mask";color:"#f07070";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter} }
                        }
                        Rectangle { anchors.right:parent.right; anchors.bottom:parent.bottom; anchors.margins:12
                            width:52; height:22; radius:4; color:"#0f1219"; border.color:"#1e2438"
                            Label { anchors.centerIn:parent; text:Math.round(root.zoom*100)+"%"; color:"#6b7a99"; font.pixelSize:11 } }
                    }

                    // Adjustments panel
                    Rectangle {
                        Layout.preferredWidth: 310; Layout.fillHeight: true
                        color:"#13161f"
                        Rectangle { anchors.left:parent.left; width:1; height:parent.height; color:"#1e2438" }
                        ScrollView { anchors.fill:parent; clip:true
                            ColumnLayout { width:310; spacing:10
                                Item { Layout.fillWidth:true; Layout.preferredHeight:46
                                    Label { anchors.left:parent.left; anchors.leftMargin:16; anchors.verticalCenter:parent.verticalCenter
                                        text:"Adjustments"; color:"#e2e8f0"; font.pixelSize:16; font.weight:Font.DemiBold } }
                                Label{text:"TRANSFORM";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                GridLayout { Layout.leftMargin:12; Layout.rightMargin:12; Layout.fillWidth:true; columns:2; rowSpacing:5; columnSpacing:5
                                    Button{text:"↺ Left";  Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.rotateCounterClockwise()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{text:"↻ Right"; Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.rotateClockwise()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{text:"⇔ Flip H";Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.flipHorizontal()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{text:"⇕ Flip V"; Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.flipVertical()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                }
                                RowLayout{Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true;spacing:5
                                    Button{Layout.fillWidth:true;text:"Undo";enabled:documentController.canUndo;implicitHeight:28;onClicked:documentController.undo()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{Layout.fillWidth:true;text:"Redo";enabled:documentController.canRedo;implicitHeight:28;onClicked:documentController.redo()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                }
                                Label{text:"LIGHT";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                AdjustmentSlider{label:"Brightness"; from:-100;to:100; value:documentController.brightness;  onMoved:(v)=>documentController.brightness =v}
                                AdjustmentSlider{label:"Exposure";   from:-3;  to:3;   value:documentController.exposure;    onMoved:(v)=>documentController.exposure   =v}
                                AdjustmentSlider{label:"Contrast";   from:-100;to:100; value:documentController.contrast;    onMoved:(v)=>documentController.contrast   =v}
                                AdjustmentSlider{label:"Highlights"; from:-100;to:100; value:documentController.highlights;  onMoved:(v)=>documentController.highlights =v}
                                AdjustmentSlider{label:"Shadows";    from:-100;to:100; value:documentController.shadows;     onMoved:(v)=>documentController.shadows    =v}
                                AdjustmentSlider{label:"Whites";     from:-100;to:100; value:documentController.whites;      onMoved:(v)=>documentController.whites     =v}
                                AdjustmentSlider{label:"Blacks";     from:-100;to:100; value:documentController.blacks;      onMoved:(v)=>documentController.blacks     =v}
                                Label{text:"COLOR";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                AdjustmentSlider{label:"Saturation"; from:-100;to:100; value:documentController.saturation;  onMoved:(v)=>documentController.saturation =v}
                                AdjustmentSlider{label:"Vibrance";   from:-100;to:100; value:documentController.vibrance;    onMoved:(v)=>documentController.vibrance   =v}
                                AdjustmentSlider{label:"Temperature";from:-100;to:100; value:documentController.temperature; onMoved:(v)=>documentController.temperature=v}
                                AdjustmentSlider{label:"Tint";       from:-100;to:100; value:documentController.tint;        onMoved:(v)=>documentController.tint       =v}
                                Label{text:"DETAIL";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                AdjustmentSlider{label:"Noise Reduction";from:0;to:100;value:documentController.noiseReduction;onMoved:(v)=>documentController.noiseReduction=v}
                                AdjustmentSlider{label:"Sharpening";     from:0;to:100;value:documentController.sharpening;    onMoved:(v)=>documentController.sharpening    =v}
                                Button{text:"Refine Edges"
                                    enabled:documentController.hasDocument&&documentController.hasMask&&!documentController.aiBusy
                                    implicitHeight:28;Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.refineEdges()
                                    ToolTip.visible:hovered;ToolTip.delay:600;ToolTip.text:"Snap mask to image edges (OpenCV)"
                                    background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                    contentItem:Label{text:"Refine Edges";color:parent.enabled?"#8892a4":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Button{text:"Reset All";enabled:documentController.hasDocument;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.resetAdjustments()
                                    background:Rectangle{color:parent.hovered?"#2a1414":"#171c2a";radius:6;border.color:"#252d45"}
                                    contentItem:Label{text:"Reset All";color:"#f07070";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Label{text:"AI TOOLS";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                Button{text:"Subject mask";enabled:documentController.hasDocument&&!documentController.aiBusy;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.requestAiMask(imagePreview.width/2,imagePreview.height/2)
                                    background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                    contentItem:Label{text:parent.text;color:parent.enabled?"#c7d2fe":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Button{text:"Object removal";enabled:documentController.hasDocument&&documentController.hasMask&&!documentController.aiBusy;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.applyInpaint()
                                    background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                    contentItem:Label{text:parent.text;color:parent.enabled?"#c7d2fe":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Button{text:"AI Upscale ×4";enabled:documentController.hasDocument&&!documentController.aiBusy;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.applyUpscale()
                                    background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                    contentItem:Label{text:parent.text;color:parent.enabled?"#c7d2fe":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Item{Layout.preferredHeight:16}
                            }
                        }
                    }
                } // RowLayout (tool+canvas+adj)
            } // Item (top split)

            // ── Bottom panel — Layers | History | Masks | Filmstrip ─────────
            Rectangle {
                id: bottomPanel
                SplitView.preferredHeight: ui.bottomHeight > 0 ? ui.bottomHeight : 160
                SplitView.minimumHeight:   30
                color: "#13161f"
                Rectangle { anchors.top:parent.top; width:parent.width; height:1; color:"#1e2438" }

                ColumnLayout {
                    anchors.fill: parent; spacing: 0

                    // Tab strip
                    Row {
                        id: tabStrip
                        Layout.fillWidth: true
                        height: 28
                        Repeater {
                            model: ["Layers","History","Masks","Filmstrip"]
                            delegate: Button {
                                text: modelData; flat: true
                                implicitWidth: 80; implicitHeight: 28
                                checked: root.bottomTab===index
                                onClicked: root.bottomTab=index
                                background: Rectangle {
                                    color: parent.checked?"#171c2a":"transparent"
                                    Rectangle { anchors.bottom:parent.bottom; width:parent.width; height:2
                                        color: parent.parent.checked?"#6366f1":"transparent" }
                                }
                                contentItem: Label { text:parent.text; font.pixelSize:11
                                    color:parent.checked?"#c8d0e0":"#4a5268"
                                    horizontalAlignment:Text.AlignHCenter }
                            }
                        }
                        Rectangle { Layout.fillWidth:true; height:1; anchors.bottom:parent.bottom; color:"transparent" }
                    }
                    Rectangle { height:1; Layout.fillWidth:true; color:"#1e2438" }

                    // Tab content
                    StackLayout {
                        currentIndex: root.bottomTab
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true

                        // ── LAYERS ──────────────────────────────────────────
                        Item {
                            ColumnLayout { anchors.fill:parent; anchors.margins:6; spacing:4
                                RowLayout {
                                    Label{text:"Layers";color:"#c8d0e0";font.pixelSize:11;font.weight:Font.DemiBold;Layout.fillWidth:true}
                                    Button{text:"+";flat:true;implicitWidth:22;implicitHeight:22;enabled:documentController.hasDocument
                                        onClicked:addLayerDialog.open()
                                        contentItem:Label{text:"+";color:"#6366f1";font.pixelSize:16;horizontalAlignment:Text.AlignHCenter}}
                                }
                                ListView { id:layerList; Layout.fillWidth:true; Layout.fillHeight:true; clip:true
                                    model: documentController.layerModel
                                    delegate: Rectangle {
                                        width:layerList.width; height:28; color:"transparent"
                                        Rectangle{anchors.bottom:parent.bottom;width:parent.width;height:1;color:"#13161f"}
                                        RowLayout{anchors.fill:parent;anchors.margins:2;spacing:3
                                            Button{text:modelData.visible?"◉":"○";flat:true;implicitWidth:22;implicitHeight:22
                                                onClicked:documentController.setLayerVisible(modelData.realId,!modelData.visible)
                                                contentItem:Label{text:parent.text;color:modelData.visible?"#6366f1":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                            Label{text:modelData.name;color:"#c8d0e0";font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight}
                                            Slider{from:0;to:1;value:modelData.opacity;implicitWidth:55;implicitHeight:18
                                                onMoved:documentController.setLayerOpacity(modelData.realId,value)}
                                            Button{text:"↑";flat:true;implicitWidth:20;implicitHeight:22
                                                onClicked:documentController.moveLayerUp(modelData.realId)
                                                contentItem:Label{text:"↑";color:"#6b7a99";font.pixelSize:12;horizontalAlignment:Text.AlignHCenter}}
                                            Button{text:"↓";flat:true;implicitWidth:20;implicitHeight:22
                                                onClicked:documentController.moveLayerDown(modelData.realId)
                                                contentItem:Label{text:"↓";color:"#6b7a99";font.pixelSize:12;horizontalAlignment:Text.AlignHCenter}}
                                            Button{text:"✕";flat:true;implicitWidth:20;implicitHeight:22
                                                onClicked:documentController.deleteLayer(modelData.realId)
                                                contentItem:Label{text:"✕";color:"#f07070";font.pixelSize:10;horizontalAlignment:Text.AlignHCenter}}
                                        }
                                    }
                                }
                            }
                        }

                        // ── HISTORY ─────────────────────────────────────────
                        Item {
                            ListView {
                                anchors.fill:parent; clip:true
                                model: documentController.historyLog
                                delegate: Rectangle {
                                    width:ListView.view.width; height:22
                                    color: index===0?"#1a2040":"transparent"
                                    RowLayout{anchors.fill:parent;anchors.leftMargin:10;anchors.rightMargin:6;spacing:8
                                        Rectangle{width:5;height:5;radius:2.5;color:index===0?"#6366f1":"#3a4566"}
                                        Label{text:modelData;color:index===0?"#c8d0e0":"#6b7a99";font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight}
                                    }
                                }
                                Label{anchors.centerIn:parent;visible:documentController.historyLog.length===0
                                    text:"No history yet";color:"#3a4566";font.pixelSize:12}
                            }
                        }

                        // ── MASKS ────────────────────────────────────────────
                        Item {
                            ColumnLayout{anchors.fill:parent;anchors.margins:6;spacing:4
                                RowLayout{
                                    Label{text:"Masks";color:"#c8d0e0";font.pixelSize:11;font.weight:Font.DemiBold;Layout.fillWidth:true}
                                    Button{text:"Clear";flat:true;implicitHeight:22;enabled:documentController.hasMask
                                        onClicked:documentController.clearMask()
                                        contentItem:Label{text:"Clear";color:"#f07070";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                }
                                ListView{Layout.fillWidth:true;Layout.fillHeight:true;clip:true
                                    model:documentController.maskList
                                    delegate:Rectangle{
                                        width:ListView.view.width;height:44;color:"#171c2a";radius:4
                                        Row{anchors.fill:parent;anchors.margins:4;spacing:8
                                            Image{width:56;height:36;fillMode:Image.PreserveAspectFit
                                                source:modelData.url||"";anchors.verticalCenter:parent.verticalCenter}
                                            Label{text:modelData.name||"Mask";color:"#c8d0e0";font.pixelSize:11;anchors.verticalCenter:parent.verticalCenter}
                                        }
                                    }
                                    Label{anchors.centerIn:parent;visible:documentController.maskList.length===0
                                        text:"No masks — paint or use gradient/radial tools";color:"#3a4566";font.pixelSize:11;wrapMode:Text.WordWrap;width:parent.width-20;horizontalAlignment:Text.AlignHCenter}
                                }
                            }
                        }

                        // ── FILMSTRIP ────────────────────────────────────────
                        Item {
                            ListView{
                                anchors.fill:parent;orientation:ListView.Horizontal;clip:true;spacing:4
                                anchors.margins:6
                                model:documentController.recentFiles
                                delegate:Rectangle{
                                    width:80;height:ListView.view.height;color:"#171c2a";radius:4
                                    border.color:documentController.sourceName===Qt.fileInfo(modelData).fileName?"#6366f1":"transparent"
                                    border.width:2
                                    Column{anchors.fill:parent;anchors.margins:4;spacing:3
                                        Image{width:parent.width;height:parent.width*0.667;fillMode:Image.PreserveAspectCrop
                                            source:"file:///"+modelData;asynchronous:true}
                                        Label{text:Qt.fileInfo(modelData).fileName||modelData;color:"#8892a4";font.pixelSize:9;
                                            width:parent.width;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter}
                                    }
                                    MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor
                                        onClicked:documentController.openImage(Qt.resolvedUrl("file:///"+modelData))}
                                }
                                Label{anchors.centerIn:parent;visible:documentController.recentFiles.length===0
                                    text:"Recently opened images appear here";color:"#3a4566";font.pixelSize:11}
                            }
                        }
                    } // StackLayout
                } // ColumnLayout (bottom)
            } // Rectangle (bottom panel)
        } // SplitView
    } // ColumnLayout (root)

    // ── AdjustmentSlider component ────────────────────────────────────────────
    // Value label is click-to-edit: single click activates a TextInput for
    // direct numeric entry (±decimal accepted). Press Enter or click away to commit.
    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real   from:  0
        property real   to:    1
        property real   value: 0
        signal moved(real nextValue)
        Layout.leftMargin:12; Layout.rightMargin:12; Layout.fillWidth:true; spacing:2
        RowLayout { Layout.fillWidth:true
            Label{text:sliderRoot.label;color:"#8892a4";font.pixelSize:11;Layout.fillWidth:true}
            Item {
                implicitWidth:52; implicitHeight:16
                Label {
                    id:valLabel; anchors.fill:parent
                    text: Number(sl.value).toFixed(sliderRoot.to<=3?2:0)
                    color: sl.value!==0?"#6366f1":"#4a5268"; font.pixelSize:11
                    horizontalAlignment:Text.AlignRight; visible:!valInput.visible
                    MouseArea { anchors.fill:parent; cursorShape:Qt.IBeamCursor
                        onClicked: { valInput.text=valLabel.text; valInput.visible=true; valInput.forceActiveFocus(); valInput.selectAll() }
                    }
                }
                TextInput {
                    id:valInput; anchors.fill:parent; visible:false
                    color:"#c7d2fe"; font.pixelSize:11; horizontalAlignment:Text.AlignRight
                    selectByMouse:true
                    validator:DoubleValidator{bottom:sliderRoot.from;top:sliderRoot.to;decimals:2;notation:DoubleValidator.StandardNotation}
                    onEditingFinished:{
                        const v=parseFloat(text.replace(",","."));
                        if (!isNaN(v)) sliderRoot.moved(Math.max(sliderRoot.from,Math.min(sliderRoot.to,v)));
                        visible=false;
                    }
                    Keys.onEscapePressed: visible=false
                }
            }
        }
        Slider { id:sl; Layout.fillWidth:true; implicitHeight:18
            from:sliderRoot.from; to:sliderRoot.to; value:sliderRoot.value
            enabled:documentController.hasDocument
            onMoved:sliderRoot.moved(value)
            background:Rectangle{x:sl.leftPadding;y:sl.topPadding+sl.availableHeight/2-height/2
                width:sl.availableWidth;height:3;radius:1.5;color:"#1a1e2e"
                Rectangle{width:sl.visualPosition*parent.width;height:parent.height;radius:1.5;color:sl.value!==0?"#6366f1":"#252d45"}}
            handle:Rectangle{
                x:sl.leftPadding+sl.visualPosition*(sl.availableWidth-width)
                y:sl.topPadding+sl.availableHeight/2-height/2
                width:13;height:13;radius:6.5
                color:sl.pressed?"#818cf8":sl.hovered?"#818cf8":"#6366f1"
                Behavior on color{ColorAnimation{duration:100}}
            }
        }
    }
} // ApplicationWindow
