import QtQuick
Item {
    id: maskCanvas
    property var    docCtrl:      null
    property double brushRadius:  50
    property bool   eraseMode:    false
    property bool   paintEnabled: false
    property real   cursorX:      0
    property real   cursorY:      0
    property bool   cursorInside: false
    property real   dragStartX:   0; property real dragStartY: 0
    property real   dragEndX:     0; property real dragEndY:   0
    property bool   draggingTool: false
    implicitWidth: 200; implicitHeight: 200

    // Draws ONLY the committed mask overlay (from the C++-saved PNG).
    // Repaints only when: the committed mask actually changes (maskChanged),
    // the image finishes loading, or the canvas resizes (the mask must be
    // redrawn at the new destination size). It does NOT repaint on mouse
    // movement -- that was the whole point of this split (see Step 1 of
    // the MaskCanvas refactor: separating committed-mask rendering from
    // the live painting overlay so the (expensive, drawImage-driven)
    // committed-mask repaint no longer runs on every mouse-move tick).
    Canvas {
        id: staticCanvas
        anchors.fill: parent
        property string loadedUrl: ""

        Connections {
            target: maskCanvas.docCtrl
            function onMaskChanged() {
                const url = maskCanvas.docCtrl && maskCanvas.docCtrl.hasMask ? maskCanvas.docCtrl.maskUrl : "";
                if (staticCanvas.loadedUrl.length > 0 && staticCanvas.loadedUrl !== url)
                    staticCanvas.unloadImage(staticCanvas.loadedUrl);
                if (url.length === 0) { staticCanvas.loadedUrl = ""; staticCanvas.requestPaint(); return; }
                staticCanvas.loadImage(url);
            }
        }
        onImageLoaded: { loadedUrl = maskCanvas.docCtrl ? maskCanvas.docCtrl.maskUrl : ""; requestPaint(); }
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const docCtrl = maskCanvas.docCtrl;

            // Committed mask overlay from C++ PNG -- identical logic to
            // the original section 1, unchanged, just hosted here now.
            if (docCtrl && docCtrl.hasDocument && docCtrl.hasMask) {
                const url = docCtrl.maskUrl;
                if (url && isImageLoaded(url)) {
                    ctx.globalAlpha = 0.42;
                    const ownerLayerId = docCtrl.activeMaskOwnerLayerId;
                    if (!ownerLayerId) {
                        ctx.drawImage(url, 0, 0, width, height);
                    } else {
                        const layers = docCtrl.layerModel;
                        let owner = null;
                        for (const l of layers) { if (l.realId === ownerLayerId) { owner = l; break; } }
                        if (owner) {
                            const canvasScale = docCtrl.sourceWidth > 0 ? width / docCtrl.sourceWidth : 1.0;
                            const ow = Math.max(1, owner.imgWidth  * owner.scaleX * canvasScale);
                            const oh = Math.max(1, owner.imgHeight * owner.scaleY * canvasScale);
                            const ocx = width  * 0.5 + owner.posX * canvasScale;
                            const ocy = height * 0.5 + owner.posY * canvasScale;
                            ctx.save();
                            ctx.translate(ocx, ocy);
                            if (owner.rotation) ctx.rotate(owner.rotation * Math.PI / 180);
                            ctx.drawImage(url, -ow / 2, -oh / 2, ow, oh);
                            ctx.restore();
                        }
                    }
                    ctx.globalAlpha = 1.0;
                } else if (url) { loadImage(url); }
            }
        }
    }

    // Draws ONLY live-interaction content: the in-progress (uncommitted)
    // stroke preview, the brush cursor ring, and the gradient/radial drag
    // previews. Repaints on every mouse move (see the MouseArea below) --
    // it never touches committed-mask state, so that per-move repaint
    // stays cheap regardless of mask size/zoom (see Step 1's comment on
    // staticCanvas for why that separation is the point of this refactor).
    Canvas {
        id: liveCanvas
        anchors.fill: parent
        onWidthChanged:  { ringSnapshot = null; requestPaint(); }
        onHeightChanged: { ringSnapshot = null; requestPaint(); }

        // Tracks the cursor ring's own last-drawn footprint (a small,
        // brush-radius-sized region -- NOT the whole canvas) so it can be
        // precisely un-drawn -- restoring whatever was underneath it,
        // accumulated brush dabs included -- before being redrawn at a new
        // position, without disturbing anything else on the canvas. This
        // is what makes it safe to stop calling ctx.reset() every paint:
        // dabs (see paintDab() below) are now painted once, directly, and
        // persist on the canvas rather than being replayed from
        // strokeHistory every frame (root cause of the long-stroke
        // slowdown: that replay was O(n) per frame, O(n^2) over a whole
        // stroke). Null when no ring is currently drawn.
        property var ringSnapshot: null

        function eraseRing() {
            if (!ringSnapshot) return;
            const ctx = getContext("2d");
            ctx.putImageData(ringSnapshot.data, ringSnapshot.x, ringSnapshot.y);
            ringSnapshot = null;
        }

        function drawRing(cx, cy, r, strokeColor, fillColor) {
            const ctx = getContext("2d");
            eraseRing();
            const m = 4; // covers the 1.5px dashed stroke + the small 2px center dot
            const x = Math.max(0, Math.floor(cx - r - m));
            const y = Math.max(0, Math.floor(cy - r - m));
            const w = Math.min(width  - x, Math.ceil(2 * (r + m)));
            const h = Math.min(height - y, Math.ceil(2 * (r + m)));
            if (w > 0 && h > 0)
                ringSnapshot = { data: ctx.getImageData(x, y, w, h), x: x, y: y };
            // Identical draw calls to the original section 3 -- only the
            // erase mechanism around them changed, not the rendering itself.
            ctx.beginPath(); ctx.arc(cx, cy, r, 0, 2*Math.PI);
            ctx.strokeStyle = strokeColor;
            ctx.lineWidth = 1.5; ctx.setLineDash([5,4]); ctx.stroke(); ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(cx, cy, 2, 0, 2*Math.PI);
            ctx.fillStyle = fillColor; ctx.fill();
        }

        // Paints ONE dab directly onto the canvas and leaves it there --
        // called once per new dab from drawLocalStroke() below, never
        // replayed. Identical drawing logic to the original strokeHistory
        // loop body; the only change is that each dab is now composited
        // exactly once (when created) instead of once per frame for the
        // rest of the stroke's duration. Because every dab is still
        // composited exactly once, in the same order, against whatever
        // was already on the canvas, the final accumulated pixels are the
        // same as the old full-replay-from-blank-every-frame approach --
        // this is not an approximation.
        function paintDab(x, y, r, erase) {
            const ctx = getContext("2d");
            const g = ctx.createRadialGradient(x, y, 0, x, y, r);
            if (erase) {
                g.addColorStop(0, "rgba(0,0,0,0.5)"); g.addColorStop(0.75,"rgba(0,0,0,0.5)"); g.addColorStop(1,"rgba(0,0,0,0)");
                ctx.globalCompositeOperation = "destination-out";
            } else {
                g.addColorStop(0,"rgba(255,255,255,0.42)"); g.addColorStop(0.75,"rgba(255,255,255,0.42)"); g.addColorStop(1,"rgba(255,255,255,0)");
                ctx.globalCompositeOperation = "source-over";
            }
            ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, r, 0, 2*Math.PI); ctx.fill();
            ctx.globalCompositeOperation = "source-over";
            // Painting directly mutates the backing store immediately, but
            // Qt Quick still needs to be told to recomposite it -- without
            // this, the dab is correctly on the canvas's pixels but never
            // gets uploaded/shown. This is cheap to call every dab (unlike
            // the old design) because onPaint no longer does any O(n)
            // work in response to it -- see onPaint below.
            requestPaint();
        }

        // Clears accumulated dab pixels (and any pending ring snapshot,
        // since it would reference now-stale pixels underneath it) once a
        // stroke ends. Needed now that dabs persist directly on the canvas
        // instead of being tied to an array that simply stops being
        // replayed -- see paintDab()/onPaint above for why.
        function clearDabs() {
            ringSnapshot = null;
            getContext("2d").clearRect(0, 0, width, height);
            // Same reasoning as paintDab(): clearRect() mutates the
            // backing store immediately, but Qt Quick still needs to be
            // told to recomposite it, or the (correctly cleared) pixels
            // never actually get shown. Self-contained here so no caller
            // can forget it -- this is exactly the bug just found in
            // onReleased's brush/erase branch, which called clearDabs()
            // without any accompanying requestPaint().
            requestPaint();
        }

        onPaint: {
            const ctx = getContext("2d");
            const docCtrl = maskCanvas.docCtrl;
            const tool = docCtrl ? docCtrl.activeTool : 0;
            const sw = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
            const sh = docCtrl ? (docCtrl.sourceHeight || height) : height;

            // 3. Brush cursor ring (tools 1 & 2). Dabs are already on the
            // canvas (painted directly by paintDab(), not replayed here) --
            // only the ring itself needs handling on every paint: erase its
            // previous footprint and redraw at the current position,
            // without touching accumulated dab pixels.
            if (tool===1 || tool===2) {
                if (maskCanvas.cursorInside) {
                    const r = maskCanvas.brushRadius * Math.min(width/sw, height/sh);
                    const color = tool===2 ? "rgba(255,100,100,0.9)" : "rgba(255,255,255,0.85)";
                    drawRing(maskCanvas.cursorX, maskCanvas.cursorY, r, color, color);
                } else {
                    eraseRing();
                }
                return;
            }

            // Tool 3/4 (gradient/radial preview) or no relevant tool active:
            // paintDab()/strokeHistory never run outside tool 1/2, so there
            // is nothing accumulated to protect -- always safe to fully clear.
            eraseRing();
            ctx.clearRect(0, 0, width, height);

            // 4. Gradient drag preview (tool 3)
            if (tool===3 && maskCanvas.draggingTool) {
                const grd=ctx.createLinearGradient(maskCanvas.dragStartX,maskCanvas.dragStartY,maskCanvas.dragEndX,maskCanvas.dragEndY);
                grd.addColorStop(0,"rgba(255,255,255,0.35)"); grd.addColorStop(1,"rgba(255,255,255,0)");
                ctx.fillStyle=grd; ctx.fillRect(0,0,width,height);
                ctx.beginPath(); ctx.moveTo(maskCanvas.dragStartX,maskCanvas.dragStartY); ctx.lineTo(maskCanvas.dragEndX,maskCanvas.dragEndY);
                ctx.strokeStyle="#fff"; ctx.lineWidth=2; ctx.setLineDash([6,4]); ctx.stroke(); ctx.setLineDash([]);
                ctx.beginPath(); ctx.arc(maskCanvas.dragStartX,maskCanvas.dragStartY,5,0,2*Math.PI); ctx.fillStyle="#fff"; ctx.fill();
                ctx.beginPath(); ctx.arc(maskCanvas.dragEndX,maskCanvas.dragEndY,5,0,2*Math.PI); ctx.strokeStyle="#fff"; ctx.lineWidth=2; ctx.stroke();
            }

            // 5. Radial drag preview (tool 4)
            if (tool===4 && maskCanvas.draggingTool) {
                const dx=maskCanvas.dragEndX-maskCanvas.dragStartX, dy=maskCanvas.dragEndY-maskCanvas.dragStartY, r=Math.sqrt(dx*dx+dy*dy);
                const grd2=ctx.createRadialGradient(maskCanvas.dragStartX,maskCanvas.dragStartY,0,maskCanvas.dragStartX,maskCanvas.dragStartY,Math.max(r,1));
                grd2.addColorStop(0,"rgba(255,255,255,0.35)"); grd2.addColorStop(0.65,"rgba(255,255,255,0.18)"); grd2.addColorStop(1,"rgba(255,255,255,0)");
                ctx.fillStyle=grd2; ctx.fillRect(0,0,width,height);
                ctx.beginPath(); ctx.arc(maskCanvas.dragStartX,maskCanvas.dragStartY,Math.max(r,1),0,2*Math.PI);
                ctx.strokeStyle="rgba(255,255,255,0.8)"; ctx.lineWidth=1.5; ctx.setLineDash([6,4]); ctx.stroke(); ctx.setLineDash([]);
                ctx.beginPath(); ctx.arc(maskCanvas.dragStartX,maskCanvas.dragStartY,4,0,2*Math.PI); ctx.fillStyle="#fff"; ctx.fill();
            }
        }
    }

    // Paints one new dab immediately (see liveCanvas.paintDab()) instead of
    // pushing to a growing history that gets replayed every frame -- this
    // is the actual long-stroke fix. brushRadius/zoom math is unchanged.
    function drawLocalStroke(x, y, erase) {
        const sw = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
        const sh = docCtrl ? (docCtrl.sourceHeight || height) : height;
        const r  = Math.max(1, brushRadius * Math.min(width/sw, height/sh));
        liveCanvas.paintDab(x, y, r, erase);
    }

    onBrushRadiusChanged: if (cursorInside) liveCanvas.requestPaint()

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property bool painting: false
        cursorShape: {
            const t = docCtrl ? docCtrl.activeTool : 0;
            if (t===1||t===2) return Qt.CrossCursor;
            if (t===3||t===4) return Qt.SizeAllCursor;
            return Qt.ArrowCursor;
        }
        onEntered:  { maskCanvas.cursorInside=true;  liveCanvas.requestPaint(); }
        onExited:   {
            maskCanvas.cursorInside=false;
            if (painting) {
                painting=false;
                liveCanvas.clearDabs();
                if (docCtrl) docCtrl.commitMaskPaint();
            } else {
                liveCanvas.requestPaint();
            }
        }
        onPressed: (mouse) => {
            if (!docCtrl) return;
            const tool = docCtrl.activeTool;
            if ((tool===1||tool===2) && !paintEnabled) return;
            if (tool===1||tool===2) {
                painting=true;
                maskCanvas.drawLocalStroke(mouse.x, mouse.y, tool===2);
                doStroke(mouse.x, mouse.y, tool===2);
            } else if (tool===3||tool===4) {
                maskCanvas.dragStartX=mouse.x; maskCanvas.dragStartY=mouse.y;
                maskCanvas.dragEndX=mouse.x;   maskCanvas.dragEndY=mouse.y;
                maskCanvas.draggingTool=true;   liveCanvas.requestPaint();
            }
        }
        onReleased: (mouse) => {
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting) {
                painting=false;
                liveCanvas.clearDabs();
                if (docCtrl) docCtrl.commitMaskPaint();
            }
            if (maskCanvas.draggingTool && docCtrl) {
                maskCanvas.draggingTool=false;
                const sw=docCtrl.sourceWidth||maskCanvas.width, sh=docCtrl.sourceHeight||maskCanvas.height;
                const x1=maskCanvas.dragStartX/maskCanvas.width*sw, y1=maskCanvas.dragStartY/maskCanvas.height*sh;
                const x2=maskCanvas.dragEndX/maskCanvas.width*sw,   y2=maskCanvas.dragEndY/maskCanvas.height*sh;
                if (tool===3) { docCtrl.applyGradientMask(x1,y1,x2,y2); }
                else if (tool===4) { const dx=x2-x1,dy=y2-y1; docCtrl.applyRadialMask(x1,y1,Math.sqrt(dx*dx+dy*dy)); }
                liveCanvas.requestPaint();
            }
        }
        onPositionChanged: (mouse) => {
            maskCanvas.cursorX=mouse.x; maskCanvas.cursorY=mouse.y;
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting && (tool===1||tool===2)) {
                maskCanvas.drawLocalStroke(mouse.x, mouse.y, tool===2);
                doStroke(mouse.x, mouse.y, tool===2);
            } else if (maskCanvas.draggingTool) {
                maskCanvas.dragEndX=mouse.x; maskCanvas.dragEndY=mouse.y; liveCanvas.requestPaint();
            } else { liveCanvas.requestPaint(); }
        }
        function doStroke(x, y, erase) {
            const sw=docCtrl.sourceWidth||maskCanvas.width, sh=docCtrl.sourceHeight||maskCanvas.height;
            docCtrl.paintMaskStroke(x/maskCanvas.width*sw, y/maskCanvas.height*sh, brushRadius, erase);
        }
    }
}