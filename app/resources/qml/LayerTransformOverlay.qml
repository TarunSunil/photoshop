import QtQuick

// Layer Transform overlay.
//
// STAGE 1 (done): click-to-select. Clicking an overlay layer's bounding box
// on the canvas sets documentController.selectedLayerId; clicking empty
// space clears it. Selected/unselected layers get a thin border so their
// clickable regions are visible while the Transform tool is active.
//
// STAGE 2 (this file, current state): drag-to-move added on top of stage 1.
// Pressing on a layer's box selects it (moved from onClicked to onPressed --
// selection now happens immediately on press, before it's known whether the
// gesture will become a drag); dragging beyond a small dead-zone moves the
// layer by calling the already-existing
// docCtrl.setLayerTransform(id, posX, posY, scaleX, scaleY, rotation)
// invokable -- only posX/posY change here, scaleX/scaleY/rotation are always
// passed through unchanged. Resize handles and a rotation handle are later
// stages, added on top of this file.
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
                id: dragArea
                anchors.fill: parent
                cursorShape: Qt.SizeAllCursor

                // Drag-to-move. Deltas are measured in `root`'s coordinate
                // space via mapToItem(), NOT this MouseArea's own local
                // space. `handle` repositions itself mid-drag in response to
                // our own setLayerTransform() calls below -- if we measured
                // in local coordinates, each tick's delta would be relative
                // to the box's already-just-moved position, silently
                // re-zeroing the reference frame every tick and making the
                // drag progressively lag behind the cursor. `root` itself
                // never moves during a drag, so it's a stable reference
                // regardless of how far the box has already travelled or
                // how it's rotated.
                property real pressRootX: 0
                property real pressRootY: 0
                property real startPosX:  0
                property real startPosY:  0
                property bool dragging:   false

                onPressed: (mouse) => {
                    if (root.docCtrl) root.docCtrl.selectedLayerId = handle.layerId;
                    const p = mapToItem(root, mouse.x, mouse.y);
                    pressRootX = p.x; pressRootY = p.y;
                    startPosX  = modelData.posX; startPosY = modelData.posY;
                    dragging   = false;
                }
                onPositionChanged: (mouse) => {
                    if (!pressed) return;
                    const p  = mapToItem(root, mouse.x, mouse.y);
                    const dx = p.x - pressRootX;
                    const dy = p.y - pressRootY;
                    // Dead-zone: a plain click (no real movement) must not
                    // emit a spurious near-zero setLayerTransform call --
                    // keeps a simple "select" click from nudging the layer
                    // by a fraction of a pixel of mouse jitter.
                    if (!dragging && Math.abs(dx) < 2 && Math.abs(dy) < 2) return;
                    dragging = true;
                    const newPosX = startPosX + dx / root.canvasScale;
                    const newPosY = startPosY + dy / root.canvasScale;
                    if (root.docCtrl)
                        root.docCtrl.setLayerTransform(handle.layerId, newPosX, newPosY,
                                                        modelData.scaleX, modelData.scaleY,
                                                        modelData.rotation);
                }
            }
        }
    }
}
