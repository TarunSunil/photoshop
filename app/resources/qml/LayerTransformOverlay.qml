import QtQuick

// Layer Transform overlay.
//
// STAGE 1 (this file, current state): click-to-select only. Clicking an
// overlay layer's bounding box on the canvas sets
// documentController.selectedLayerId; clicking empty space clears it.
// Selected/unselected layers get a thin border so their clickable regions
// are visible while the Transform tool is active. No dragging, resizing, or
// rotation yet -- those are later stages, added on top of this file once
// selection itself is verified working end to end.
//
// Visible only when documentController.activeTool === 6 ("Transform"),
// placed over the image preview at the same size as MaskCanvas/CropOverlay
// (anchors.centerIn + matching width/height bound to imagePreview -- see
// Main.qml).
//
// Hit-testing deliberately uses native QML Item `rotation` +
// `transformOrigin` instead of manual matrix math: each per-layer Item below
// is positioned/sized/rotated using the exact same posX/posY/scaleX/scaleY/
// rotation convention RenderPipeline::compositeOverlayLayers() already uses
// to composite the layer for real (posX/posY in base-image pixels relative
// to canvas center; scaleX/scaleY relative to the layer's own native pixel
// size; rotation in degrees, clockwise). Because the Item's own `rotation`
// property is what's rotated -- the same mechanism Qt Quick uses for its own
// rendering -- its MouseArea's hit box automatically respects that rotation.
// There is no separate rotated-hit-test math to keep in sync with the
// render path.
Item {
    id: root
    property var docCtrl: null

    // Canvas-pixels-per-source-pixel. This overlay is sized to match
    // imagePreview (documentController.sourceWidth/Height * zoom), so this
    // recovers the same "scale" factor RenderPipeline::compositeOverlayLayers()
    // calls `previewScale` -- uniform in X and Y since aspect ratio is
    // always preserved when the preview image is sized.
    readonly property real canvasScale: (docCtrl && docCtrl.sourceWidth > 0)
        ? width / docCtrl.sourceWidth : 1.0

    // documentController.layerModel is topmost-first (index 0 = highest
    // order/most-recently-added-on-top). Reverse the list and drop the base
    // layer (order 0, isBase -- not transformable) so the Repeater below
    // adds the lowest-order overlay FIRST and the highest-order overlay
    // LAST. Later Repeater siblings paint on top and win mouse-event
    // priority when two overlays' boxes overlap, matching the actual visual
    // stacking order the user sees.
    readonly property var overlayModel: {
        const list = docCtrl ? docCtrl.layerModel : [];
        const result = [];
        for (let i = list.length - 1; i >= 0; --i)
            if (!list[i].isBase) result.push(list[i]);
        return result;
    }

    // Clicking empty space (not on any layer's box) deselects. Declared
    // BEFORE the Repeater so its per-layer Items paint on top and take
    // click priority over this background catch-all.
    MouseArea {
        anchors.fill: parent
        onClicked: if (root.docCtrl) root.docCtrl.selectedLayerId = ""
    }

    Repeater {
        model: root.overlayModel
        delegate: Item {
            id: handle
            readonly property string layerId: modelData.realId
            readonly property bool   isSelected: root.docCtrl && root.docCtrl.selectedLayerId === layerId

            width:  Math.max(1, modelData.imgWidth  * modelData.scaleX * root.canvasScale)
            height: Math.max(1, modelData.imgHeight * modelData.scaleY * root.canvasScale)
            x: (root.width  * 0.5 + modelData.posX * root.canvasScale) - width  * 0.5
            y: (root.height * 0.5 + modelData.posY * root.canvasScale) - height * 0.5
            rotation: modelData.rotation
            transformOrigin: Item.Center

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: handle.isSelected ? 2 : 1
                border.color: handle.isSelected ? "#6366f1" : "#ffffff33"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: if (root.docCtrl) root.docCtrl.selectedLayerId = handle.layerId
            }
        }
    }
}
