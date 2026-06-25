import QtQuick
Canvas {
    id: maskCanvas
    property var    docCtrl:       null
    property double brushRadius:   50
    property bool   eraseMode:     false
    property bool   paintEnabled:  false
    property bool   isPainting:    false   // true while mouse button held on brush stroke
    property string loadedUrl:     ""
    property real   cursorX:       0
    property real   cursorY:       0
    property bool   cursorInside:  false
    // Gradient / radial drag state
    property real   dragStartX:    0
    property real   dragStartY:    0
    property real   dragEndX:      0
    property real   dragEndY:      0
    property bool   draggingTool:  false

    implicitWidth: 200; implicitHeight: 200

    // ── Mask reload from C++ ──────────────────────────────────────────────────
    // Skipped while the user is actively painting — the local canvas already shows
    // the strokes drawn by drawLocalStroke().  Reload happens once on commit
    // (mouseRelease → commitMaskPaint → maskChanged with isPainting==false).
    Connections {
        target: docCtrl
        function onMaskChanged() {
            if (maskCanvas.isPainting) return;
            const url = docCtrl && docCtrl.hasMask ? docCtrl.maskUrl : "";
            if (maskCanvas.loadedUrl.length > 0 && maskCanvas.loadedUrl !== url)
                maskCanvas.unloadImage(maskCanvas.loadedUrl);
            if (url.length === 0) {
                maskCanvas.loadedUrl = "";
                maskCanvas.requestPaint();
                return;
            }
            maskCanvas.loadImage(url);
        }
    }
    onImageLoaded: { loadedUrl = docCtrl ? docCtrl.maskUrl : ""; requestPaint(); }

    // ── Full repaint ──────────────────────────────────────────────────────────
    // During active painting we return immediately without calling ctx.reset().
    // The canvas buffer already contains the committed mask (drawn on the last
    // full repaint before painting started) plus all strokes added by
    // drawLocalStroke().  Returning without reset preserves that content so
    // strokes remain visible across frames without any file I/O.
    onPaint: {
        if (isPainting) return;

        const ctx = getContext("2d");
        ctx.reset();
        const tool = docCtrl ? docCtrl.activeTool : 0;

        // Committed mask overlay (from C++ PNG)
        if (docCtrl && docCtrl.hasDocument && docCtrl.hasMask) {
            const url = docCtrl.maskUrl;
            if (url && isImageLoaded(url)) {
                ctx.globalAlpha = 0.42;
                ctx.drawImage(url, 0, 0, width, height);
                ctx.globalAlpha = 1.0;
            } else if (url) { loadImage(url); }
        }

        const sw  = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
        const sh  = docCtrl ? (docCtrl.sourceHeight || height) : height;
        const scx = width  / sw;
        const scy = height / sh;

        // Brush cursor (tools 1 & 2) — only shown when not actively painting
        if ((tool === 1 || tool === 2) && cursorInside) {
            const r = brushRadius * Math.min(scx, scy);
            ctx.beginPath();
            ctx.arc(cursorX, cursorY, r, 0, 2 * Math.PI);
            ctx.strokeStyle = tool === 2 ? "rgba(255,100,100,0.9)" : "rgba(255,255,255,0.85)";
            ctx.lineWidth = 1.5;
            ctx.setLineDash([5, 4]);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.beginPath();
            ctx.arc(cursorX, cursorY, 2, 0, 2 * Math.PI);
            ctx.fillStyle = tool === 2 ? "rgba(255,100,100,0.9)" : "rgba(255,255,255,0.85)";
            ctx.fill();
        }

        // Gradient tool drag preview (tool 3)
        if (tool === 3 && draggingTool) {
            const grd = ctx.createLinearGradient(dragStartX, dragStartY, dragEndX, dragEndY);
            grd.addColorStop(0, "rgba(255,255,255,0.35)");
            grd.addColorStop(1, "rgba(255,255,255,0.0)");
            ctx.fillStyle = grd;
            ctx.fillRect(0, 0, width, height);
            ctx.beginPath();
            ctx.moveTo(dragStartX, dragStartY);
            ctx.lineTo(dragEndX, dragEndY);
            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 2;
            ctx.setLineDash([6, 4]); ctx.stroke(); ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(dragStartX, dragStartY, 5, 0, 2*Math.PI);
            ctx.fillStyle = "#ffffff"; ctx.fill();
            ctx.beginPath(); ctx.arc(dragEndX, dragEndY, 5, 0, 2*Math.PI);
            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 2; ctx.stroke();
        }

        // Radial tool drag preview (tool 4)
        if (tool === 4 && draggingTool) {
            const dx = dragEndX - dragStartX, dy = dragEndY - dragStartY;
            const r  = Math.sqrt(dx*dx + dy*dy);
            const grd2 = ctx.createRadialGradient(dragStartX, dragStartY, 0,
                                                   dragStartX, dragStartY, Math.max(r, 1));
            grd2.addColorStop(0,    "rgba(255,255,255,0.35)");
            grd2.addColorStop(0.65, "rgba(255,255,255,0.18)");
            grd2.addColorStop(1,    "rgba(255,255,255,0.0)");
            ctx.fillStyle = grd2; ctx.fillRect(0, 0, width, height);
            ctx.beginPath();
            ctx.arc(dragStartX, dragStartY, Math.max(r, 1), 0, 2*Math.PI);
            ctx.strokeStyle = "rgba(255,255,255,0.8)"; ctx.lineWidth = 1.5;
            ctx.setLineDash([6, 4]); ctx.stroke(); ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(dragStartX, dragStartY, 4, 0, 2*Math.PI);
            ctx.fillStyle = "#fff"; ctx.fill();
        }
    }

    // ── Direct stroke drawing (instant visual feedback) ───────────────────────
    // Writes directly to the 2D canvas buffer — no requestPaint(), no ctx.reset().
    // The result is visible in the very next GPU frame, giving sub-frame latency.
    // Opacity 0.42 matches the committed mask overlay so the transition on commit
    // is visually seamless.
    function drawLocalStroke(x, y, erase) {
        const ctx = getContext("2d");
        const sw  = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
        const sh  = docCtrl ? (docCtrl.sourceHeight || height) : height;
        const r   = Math.max(1, brushRadius * Math.min(width / sw, height / sh));
        const gradient = ctx.createRadialGradient(x, y, 0, x, y, r);
        if (erase) {
            gradient.addColorStop(0,    "rgba(0,0,0,0.5)");
            gradient.addColorStop(0.75, "rgba(0,0,0,0.5)");
            gradient.addColorStop(1,    "rgba(0,0,0,0)");
            ctx.globalCompositeOperation = "destination-out";
        } else {
            gradient.addColorStop(0,    "rgba(255,255,255,0.42)");
            gradient.addColorStop(0.75, "rgba(255,255,255,0.42)");
            gradient.addColorStop(1,    "rgba(255,255,255,0)");
            ctx.globalCompositeOperation = "source-over";
        }
        ctx.globalAlpha = 1.0;
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.arc(x, y, r, 0, 2 * Math.PI);
        ctx.fill();
        ctx.globalCompositeOperation = "source-over";
    }

    // Suppress resize repaints while painting (they would clear local strokes)
    onWidthChanged:  { if (!isPainting) requestPaint(); }
    onHeightChanged: { if (!isPainting) requestPaint(); }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property bool painting: false

        cursorShape: {
            const t = docCtrl ? docCtrl.activeTool : 0;
            if (t === 1 || t === 2) return Qt.CrossCursor;
            if (t === 3 || t === 4) return Qt.SizeAllCursor;
            return Qt.ArrowCursor;
        }

        onEntered:  { maskCanvas.cursorInside = true;  maskCanvas.requestPaint(); }
        onExited:   {
            maskCanvas.cursorInside = false;
            if (painting) {
                painting = false;
                maskCanvas.isPainting = false;
                if (docCtrl) docCtrl.commitMaskPaint();
            }
            maskCanvas.requestPaint();
        }

        onPressed: (mouse) => {
            if (!paintEnabled || !docCtrl) return;
            const tool = docCtrl.activeTool;
            if (tool === 1 || tool === 2) {
                painting = true;
                maskCanvas.isPainting = true;
                // Draw first stroke directly — instant, no requestPaint needed
                maskCanvas.drawLocalStroke(mouse.x, mouse.y, tool === 2);
                doStroke(mouse.x, mouse.y, tool === 2);
            } else if (tool === 3 || tool === 4) {
                maskCanvas.dragStartX = mouse.x; maskCanvas.dragStartY = mouse.y;
                maskCanvas.dragEndX   = mouse.x; maskCanvas.dragEndY   = mouse.y;
                maskCanvas.draggingTool = true;
                maskCanvas.requestPaint();
            }
        }

        onReleased: (mouse) => {
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting) {
                painting = false;
                maskCanvas.isPainting = false;
                if (docCtrl) docCtrl.commitMaskPaint();
                // commitMaskPaint emits maskChanged with isPainting==false
                // → onMaskChanged reloads from C++ PNG → requestPaint → full repaint
            }
            if (maskCanvas.draggingTool && docCtrl) {
                maskCanvas.draggingTool = false;
                const sw = docCtrl.sourceWidth  || maskCanvas.width;
                const sh = docCtrl.sourceHeight || maskCanvas.height;
                const x1 = maskCanvas.dragStartX / maskCanvas.width  * sw;
                const y1 = maskCanvas.dragStartY / maskCanvas.height * sh;
                const x2 = maskCanvas.dragEndX   / maskCanvas.width  * sw;
                const y2 = maskCanvas.dragEndY   / maskCanvas.height * sh;
                if (tool === 3) {
                    docCtrl.applyGradientMask(x1, y1, x2, y2);
                } else if (tool === 4) {
                    const dx = x2 - x1, dy = y2 - y1;
                    docCtrl.applyRadialMask(x1, y1, Math.sqrt(dx*dx + dy*dy));
                }
                maskCanvas.requestPaint();
            }
        }

        onPositionChanged: (mouse) => {
            maskCanvas.cursorX = mouse.x; maskCanvas.cursorY = mouse.y;
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting && (tool === 1 || tool === 2)) {
                // Draw directly — do NOT call requestPaint (that resets and loses strokes)
                maskCanvas.drawLocalStroke(mouse.x, mouse.y, tool === 2);
                doStroke(mouse.x, mouse.y, tool === 2);
            } else if (maskCanvas.draggingTool) {
                maskCanvas.dragEndX = mouse.x; maskCanvas.dragEndY = mouse.y;
                maskCanvas.requestPaint();
            } else {
                maskCanvas.requestPaint(); // cursor-circle update only
            }
        }

        function doStroke(x, y, erase) {
            const sw = docCtrl.sourceWidth  || maskCanvas.width;
            const sh = docCtrl.sourceHeight || maskCanvas.height;
            docCtrl.paintMaskStroke(
                x / maskCanvas.width  * sw,
                y / maskCanvas.height * sh,
                brushRadius, erase);
        }
    }
}
