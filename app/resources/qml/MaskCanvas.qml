import QtQuick
Canvas {
    id: maskCanvas
    property var    docCtrl:       null
    property double brushRadius:   50
    property bool   eraseMode:     false
    property bool   paintEnabled:  false
    property string loadedUrl:     ""
    property real   cursorX:       0
    property real   cursorY:       0
    property bool   cursorInside:  false
    // Gradient/radial drag state
    property real   dragStartX:    0
    property real   dragStartY:    0
    property real   dragEndX:      0
    property real   dragEndY:      0
    property bool   draggingTool:  false
    implicitWidth: 200; implicitHeight: 200
    Connections {
        target: docCtrl
        function onMaskChanged() {
            const url = docCtrl && docCtrl.hasMask ? docCtrl.maskUrl : "";
            if (maskCanvas.loadedUrl.length > 0 && maskCanvas.loadedUrl !== url)
                maskCanvas.unloadImage(maskCanvas.loadedUrl);
            if (url.length === 0) { maskCanvas.loadedUrl = ""; maskCanvas.requestPaint(); return; }
            maskCanvas.loadImage(url);
        }
    }
    onImageLoaded: { loadedUrl = docCtrl ? docCtrl.maskUrl : ""; requestPaint(); }
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const tool = docCtrl ? docCtrl.activeTool : 0;
        // ── Mask overlay ──────────────────────────────────────────────
        if (docCtrl && docCtrl.hasDocument && docCtrl.hasMask) {
            const url = docCtrl.maskUrl;
            if (url && isImageLoaded(url)) {
                ctx.globalAlpha = 0.42;
                ctx.drawImage(url, 0, 0, width, height);
                ctx.globalAlpha = 1.0;
            } else if (url) { loadImage(url); }
        }
        const sw = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
        const sh = docCtrl ? (docCtrl.sourceHeight || height) : height;
        const scx = width  / sw;
        const scy = height / sh;
        // ── Brush cursor (tools 1 & 2) ────────────────────────────────
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
        // ── Gradient tool preview (tool 3) ───────────────────────────
        if (tool === 3 && draggingTool) {
            const grd = ctx.createLinearGradient(dragStartX, dragStartY, dragEndX, dragEndY);
            grd.addColorStop(0, "rgba(255,255,255,0.35)");
            grd.addColorStop(1, "rgba(255,255,255,0.0)");
            ctx.fillStyle = grd;
            ctx.fillRect(0, 0, width, height);
            // Arrow line
            ctx.beginPath();
            ctx.moveTo(dragStartX, dragStartY);
            ctx.lineTo(dragEndX, dragEndY);
            ctx.strokeStyle = "#ffffff";
            ctx.lineWidth = 2;
            ctx.setLineDash([6, 4]);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(dragStartX, dragStartY, 5, 0, 2*Math.PI);
            ctx.fillStyle = "#ffffff"; ctx.fill();
            ctx.beginPath(); ctx.arc(dragEndX, dragEndY, 5, 0, 2*Math.PI);
            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 2; ctx.stroke();
        }
        // ── Radial tool preview (tool 4) ─────────────────────────────
        if (tool === 4 && draggingTool) {
            const dx = dragEndX - dragStartX, dy = dragEndY - dragStartY;
            const r = Math.sqrt(dx*dx + dy*dy);
            const grd2 = ctx.createRadialGradient(dragStartX, dragStartY, 0, dragStartX, dragStartY, Math.max(r, 1));
            grd2.addColorStop(0,    "rgba(255,255,255,0.35)");
            grd2.addColorStop(0.65, "rgba(255,255,255,0.18)");
            grd2.addColorStop(1,    "rgba(255,255,255,0.0)");
            ctx.fillStyle = grd2;
            ctx.fillRect(0, 0, width, height);
            ctx.beginPath();
            ctx.arc(dragStartX, dragStartY, Math.max(r, 1), 0, 2*Math.PI);
            ctx.strokeStyle = "rgba(255,255,255,0.8)";
            ctx.lineWidth = 1.5;
            ctx.setLineDash([6, 4]);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(dragStartX, dragStartY, 4, 0, 2*Math.PI);
            ctx.fillStyle = "#fff"; ctx.fill();
        }
        // ── Crop overlay (tool 5) ────────────────────────────────────
        if (tool === 5 && draggingTool) {
            const cx = Math.min(dragStartX, dragEndX);
            const cy = Math.min(dragStartY, dragEndY);
            const cw = Math.abs(dragEndX - dragStartX);
            const ch = Math.abs(dragEndY - dragStartY);
            // Darken outside crop
            ctx.fillStyle = "rgba(0,0,0,0.5)";
            ctx.fillRect(0, 0, width, height);
            ctx.clearRect(cx, cy, cw, ch);
            // Border
            ctx.strokeStyle = "#ffffff";
            ctx.lineWidth = 1.5;
            ctx.setLineDash([]);
            ctx.strokeRect(cx, cy, cw, ch);
            // Rule of thirds
            ctx.strokeStyle = "rgba(255,255,255,0.3)";
            ctx.lineWidth = 1;
            ctx.setLineDash([3,3]);
            ctx.beginPath();
            ctx.moveTo(cx + cw/3, cy); ctx.lineTo(cx + cw/3, cy + ch);
            ctx.moveTo(cx + 2*cw/3, cy); ctx.lineTo(cx + 2*cw/3, cy + ch);
            ctx.moveTo(cx, cy + ch/3); ctx.lineTo(cx + cw, cy + ch/3);
            ctx.moveTo(cx, cy + 2*ch/3); ctx.lineTo(cx + cw, cy + 2*ch/3);
            ctx.stroke();
            ctx.setLineDash([]);
        }
    }
    onWidthChanged:  requestPaint()
    onHeightChanged: requestPaint()
    MouseArea {
        anchors.fill: parent
        // Left button only — right-click passes through to the pan handler above
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property bool painting: false
        cursorShape: {
            const t = docCtrl ? docCtrl.activeTool : 0;
            if (t === 1) return Qt.CrossCursor;
            if (t === 2) return Qt.CrossCursor;
            if (t === 3) return Qt.SizeAllCursor;
            if (t === 4) return Qt.SizeAllCursor;
            if (t === 5) return Qt.CrossCursor;
            return Qt.ArrowCursor;
        }
        onEntered:  { maskCanvas.cursorInside = true;  maskCanvas.requestPaint(); }
        onExited:   {
            maskCanvas.cursorInside = false;
            painting = false;
            // Flush any pending stroke on exit
            if (docCtrl) docCtrl.commitMaskPaint();
            maskCanvas.requestPaint();
        }
        onPressed: (mouse) => {
            if (!paintEnabled || !docCtrl) return;
            const tool = docCtrl.activeTool;
            if (tool === 1 || tool === 2) {
                painting = true;
                doStroke(mouse.x, mouse.y, tool === 2);
            } else if (tool === 3 || tool === 4 || tool === 5) {
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
                if (docCtrl) docCtrl.commitMaskPaint();
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
                } else if (tool === 5) {
                    const cx = Math.round(Math.min(x1,x2));
                    const cy = Math.round(Math.min(y1,y2));
                    const cw = Math.round(Math.abs(x2-x1));
                    const ch = Math.round(Math.abs(y2-y1));
                    if (cw > 10 && ch > 10) docCtrl.applyCrop(cx, cy, cw, ch);
                }
                maskCanvas.requestPaint();
            }
        }
        onPositionChanged: (mouse) => {
            maskCanvas.cursorX = mouse.x; maskCanvas.cursorY = mouse.y;
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting && (tool === 1 || tool === 2)) {
                doStroke(mouse.x, mouse.y, tool === 2);
            } else if (maskCanvas.draggingTool) {
                maskCanvas.dragEndX = mouse.x; maskCanvas.dragEndY = mouse.y;
                maskCanvas.requestPaint();
            } else {
                maskCanvas.requestPaint(); // update cursor circle
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