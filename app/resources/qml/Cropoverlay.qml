import QtQuick
import QtQuick.Controls

// Resizable, draggable crop overlay. Placed over the image preview at the same
// size. Visible only when documentController.activeTool === 5.
//
// Coordinate system: boxX/Y/W/H are in canvas pixels (same space as width/height).
// On confirm, they are scaled to source-image space before calling applyCrop().
Item {
    id: cropOverlay

    property var  docCtrl:  null
    property real minSize:  40       // minimum side length in canvas pixels

    property real boxX: 0
    property real boxY: 0
    property real boxW: width
    property real boxH: height
    property real rotation: 0

    // Reset to 90% of the canvas when the overlay appears
    onVisibleChanged: {
        if (visible) {
            boxX = width  * 0.05;
            boxY = height * 0.05;
            boxW = width  * 0.90;
            boxH = height * 0.90;
        }
    }

    // Called by the Enter shortcut in Main.qml and by the Crop button
    function confirm() {
        if (!docCtrl) return;
        const sw = docCtrl.sourceWidth;
        const sh = docCtrl.sourceHeight;
        docCtrl.applyCrop(
            Math.round(boxX / width  * sw),
            Math.round(boxY / height * sh),
            Math.round(boxW / width  * sw),
            Math.round(boxH / height * sh),
            rotation
        );
    }

    // ── Dark vignette outside the crop box (four rectangles) ─────────────────
    Rectangle {
        x: 0; y: 0
        width: parent.width; height: cropOverlay.boxY
        color: Qt.rgba(0, 0, 0, 0.55)
    }
    Rectangle {
        x: 0
        y: cropOverlay.boxY + cropOverlay.boxH
        width: parent.width
        height: Math.max(0, parent.height - cropOverlay.boxY - cropOverlay.boxH)
        color: Qt.rgba(0, 0, 0, 0.55)
    }
    Rectangle {
        x: 0; y: cropOverlay.boxY
        width: cropOverlay.boxX; height: cropOverlay.boxH
        color: Qt.rgba(0, 0, 0, 0.55)
    }
    Rectangle {
        x: cropOverlay.boxX + cropOverlay.boxW; y: cropOverlay.boxY
        width: Math.max(0, parent.width - cropOverlay.boxX - cropOverlay.boxW)
        height: cropOverlay.boxH
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    // ── Crop-box border ───────────────────────────────────────────────────────
    Rectangle {
        id: cropBorder
        x: cropOverlay.boxX; y: cropOverlay.boxY
        width: cropOverlay.boxW; height: cropOverlay.boxH
        color: "transparent"
        border.color: "white"; border.width: 1
        rotation: cropOverlay.rotation
        transformOrigin: Item.Center

        // Rule-of-thirds grid — native Rectangle lines (GPU-composited), not a
        // Canvas. The previous Canvas-based version did a full CPU
        // ctx.reset() + 4-line redraw on every pixel of a resize-drag (its
        // onWidthChanged/onHeightChanged fired every frame), which is a real,
        // known source of jank for Canvas items during continuous resizing.
        // Plain Rectangles let the scene graph move/resize them with zero
        // CPU rasterization.
        Item {
            anchors.fill: parent
            opacity: 0.35
            // Two vertical thirds lines
            Repeater {
                model: 2
                Rectangle {
                    x: parent.width * (index + 1) / 3 - width / 2
                    y: 0
                    width: 1
                    height: parent.height
                    color: "white"
                }
            }
            // Two horizontal thirds lines
            Repeater {
                model: 2
                Rectangle {
                    x: 0
                    y: parent.height * (index + 1) / 3 - height / 2
                    width: parent.width
                    height: 1
                    color: "white"
                }
            }
        }

        // Interior drag — moves the entire box.
        // margins:12 leaves a dead zone so the handle MouseAreas get priority
        // at corners and edges where they overlap with the border area.
        MouseArea {
            anchors { fill: parent; margins: 12 }
            cursorShape: Qt.SizeAllCursor
            property real sx: 0; property real sy: 0
            property real sbx: 0; property real sby: 0
            onPressed:  (mouse) => {
                sx = mouse.x; sy = mouse.y;
                sbx = cropOverlay.boxX; sby = cropOverlay.boxY;
            }
            onPositionChanged: (mouse) => {
                const dx = mouse.x - sx, dy = mouse.y - sy;
                cropOverlay.boxX = Math.max(0, Math.min(cropOverlay.width  - cropOverlay.boxW, sbx + dx));
                cropOverlay.boxY = Math.max(0, Math.min(cropOverlay.height - cropOverlay.boxH, sby + dy));
            }
        }
    }

    // ── Eight resize handles ──────────────────────────────────────────────────
    // Index:  0=NW  1=N  2=NE  3=E  4=SE  5=S  6=SW  7=W
    Repeater {
        model: 8
        delegate: Rectangle {
            id: handleRect
            readonly property int  idx:      index
            readonly property bool onLeft:   idx === 0 || idx === 6 || idx === 7
            readonly property bool onRight:  idx === 2 || idx === 3 || idx === 4
            readonly property bool onTop:    idx === 0 || idx === 1 || idx === 2
            readonly property bool onBottom: idx === 4 || idx === 5 || idx === 6
            readonly property real localHandleX:
                onLeft ? -cropOverlay.boxW / 2 :
                onRight ? cropOverlay.boxW / 2 : 0
            readonly property real localHandleY:
                onTop ? -cropOverlay.boxH / 2 :
                onBottom ? cropOverlay.boxH / 2 : 0
            readonly property real handleCos: Math.cos(cropOverlay.rotation * Math.PI / 180)
            readonly property real handleSin: Math.sin(cropOverlay.rotation * Math.PI / 180)

            x: cropOverlay.boxX + cropOverlay.boxW / 2
               + localHandleX * handleCos - localHandleY * handleSin - width / 2
            y: cropOverlay.boxY + cropOverlay.boxH / 2
               + localHandleX * handleSin + localHandleY * handleCos - height / 2

            width: 10; height: 10; radius: 2
            color: "white"
            z: 10   // above cropBorder

            MouseArea {
                anchors.fill: parent
                cursorShape: {
                    if (handleRect.onLeft  && handleRect.onTop)    return Qt.SizeFDiagCursor;
                    if (handleRect.onRight && handleRect.onBottom) return Qt.SizeFDiagCursor;
                    if (handleRect.onRight && handleRect.onTop)    return Qt.SizeBDiagCursor;
                    if (handleRect.onLeft  && handleRect.onBottom) return Qt.SizeBDiagCursor;
                    if (handleRect.onTop   || handleRect.onBottom) return Qt.SizeVerCursor;
                    return Qt.SizeHorCursor;
                }
                property real startX:  0; property real startY:  0
                property real startBX: 0; property real startBY: 0
                property real startBW: 0; property real startBH: 0

                onPressed: (mouse) => {
                    startX  = mouse.x; startY  = mouse.y;
                    startBX = cropOverlay.boxX; startBY = cropOverlay.boxY;
                    startBW = cropOverlay.boxW; startBH = cropOverlay.boxH;
                }
                onPositionChanged: (mouse) => {
                    const dx = mouse.x - startX;
                    const dy = mouse.y - startY;
                    let nx = startBX, ny = startBY;
                    let nw = startBW, nh = startBH;

                    // Left edge
                    if (handleRect.onLeft) {
                        nw = Math.max(cropOverlay.minSize, startBW - dx);
                        nx = startBX + startBW - nw;
                    }
                    // Right edge
                    if (handleRect.onRight) {
                        nw = Math.max(cropOverlay.minSize, startBW + dx);
                    }
                    // Top edge
                    if (handleRect.onTop) {
                        nh = Math.max(cropOverlay.minSize, startBH - dy);
                        ny = startBY + startBH - nh;
                    }
                    // Bottom edge
                    if (handleRect.onBottom) {
                        nh = Math.max(cropOverlay.minSize, startBH + dy);
                    }

                    // Clamp to canvas bounds
                    nx = Math.max(0, nx);
                    ny = Math.max(0, ny);
                    nw = Math.min(nw, cropOverlay.width  - nx);
                    nh = Math.min(nh, cropOverlay.height - ny);

                    cropOverlay.boxX = nx; cropOverlay.boxY = ny;
                    cropOverlay.boxW = nw; cropOverlay.boxH = nh;
                }
            }
        }
    }

    // ── Confirm / Cancel buttons ──────────────────────────────────────────────
    Row {
        id: actionRow
        // Centre below the crop box; clamp so it stays on screen
        x: Math.round(cropOverlay.boxX + (cropOverlay.boxW - width) * 0.5)
        y: Math.min(cropOverlay.boxY + cropOverlay.boxH + 12,
                    cropOverlay.height - height - 8)
        spacing: 8
        z: 20

        Button {
            text: "\u21ba"
            implicitWidth: 30; implicitHeight: 30
            onClicked: cropOverlay.rotation -= 1
            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 5; border.color: "#252d45" }
            contentItem: Label { text: parent.text; color: "#c8d0e0"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter }
        }
        Button {
            text: "\u21bb"
            implicitWidth: 30; implicitHeight: 30
            onClicked: cropOverlay.rotation += 1
            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 5; border.color: "#252d45" }
            contentItem: Label { text: parent.text; color: "#c8d0e0"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter }
        }
        Button {
            text: "\u2713  Crop"
            implicitWidth: 86; implicitHeight: 30
            onClicked: cropOverlay.confirm()
            background: Rectangle {
                color: parent.hovered ? "#252d6a" : "#1c2058"
                radius: 7; border.color: "#3d41a0"
            }
            contentItem: Label {
                text: "\u2713  Crop"; color: "#c7d2fe"
                font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
            }
        }
        Button {
            text: "\u2715  Cancel"
            implicitWidth: 86; implicitHeight: 30
            onClicked: { if (cropOverlay.docCtrl) cropOverlay.docCtrl.activeTool = 0; }
            background: Rectangle {
                color: parent.hovered ? "#1e2438" : "#171c2a"
                radius: 7; border.color: "#252d45"
            }
            contentItem: Label {
                text: "\u2715  Cancel"; color: "#8892a4"
                font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
