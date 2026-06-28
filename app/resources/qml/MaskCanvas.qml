import QtQuick
Canvas {
    id: maskCanvas
    property var    docCtrl:      null
    property double brushRadius:  50
    property bool   eraseMode:    false
    property bool   paintEnabled: false
    property var    strokeHistory: []
    property string loadedUrl:    ""
    property real   cursorX:      0
    property real   cursorY:      0
    property bool   cursorInside: false
    property real   dragStartX:   0; property real dragStartY: 0
    property real   dragEndX:     0; property real dragEndY:   0
    property bool   draggingTool: false
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

    // onPaint always does a FULL reset + replay.
    // strokeHistory stores locally-drawn strokes so they appear before commit.
    // This gives <1 frame (~16ms) latency with no stale canvas state.
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const tool = docCtrl ? docCtrl.activeTool : 0;
        const sw = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
        const sh = docCtrl ? (docCtrl.sourceHeight || height) : height;

        // 1. Committed mask overlay from C++ PNG
        if (docCtrl && docCtrl.hasDocument && docCtrl.hasMask) {
            const url = docCtrl.maskUrl;
            if (url && isImageLoaded(url)) {
                ctx.globalAlpha = 0.42;
                ctx.drawImage(url, 0, 0, width, height);
                ctx.globalAlpha = 1.0;
            } else if (url) { loadImage(url); }
        }

        // 2. Locally accumulated brush strokes (not yet committed to C++)
        for (const s of maskCanvas.strokeHistory) {
            const g = ctx.createRadialGradient(s.x, s.y, 0, s.x, s.y, s.r);
            if (s.erase) {
                g.addColorStop(0, "rgba(0,0,0,0.5)"); g.addColorStop(0.75,"rgba(0,0,0,0.5)"); g.addColorStop(1,"rgba(0,0,0,0)");
                ctx.globalCompositeOperation = "destination-out";
            } else {
                g.addColorStop(0,"rgba(255,255,255,0.42)"); g.addColorStop(0.75,"rgba(255,255,255,0.42)"); g.addColorStop(1,"rgba(255,255,255,0)");
                ctx.globalCompositeOperation = "source-over";
            }
            ctx.fillStyle = g; ctx.beginPath(); ctx.arc(s.x,s.y,s.r,0,2*Math.PI); ctx.fill();
            ctx.globalCompositeOperation = "source-over";
        }

        // 3. Brush cursor ring (tools 1 & 2)
        if ((tool===1||tool===2) && cursorInside) {
            const r = brushRadius * Math.min(width/sw, height/sh);
            ctx.beginPath(); ctx.arc(cursorX,cursorY,r,0,2*Math.PI);
            ctx.strokeStyle = tool===2 ? "rgba(255,100,100,0.9)" : "rgba(255,255,255,0.85)";
            ctx.lineWidth = 1.5; ctx.setLineDash([5,4]); ctx.stroke(); ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(cursorX,cursorY,2,0,2*Math.PI);
            ctx.fillStyle = tool===2 ? "rgba(255,100,100,0.9)" : "rgba(255,255,255,0.85)"; ctx.fill();
        }

        // 4. Gradient drag preview (tool 3)
        if (tool===3 && draggingTool) {
            const grd=ctx.createLinearGradient(dragStartX,dragStartY,dragEndX,dragEndY);
            grd.addColorStop(0,"rgba(255,255,255,0.35)"); grd.addColorStop(1,"rgba(255,255,255,0)");
            ctx.fillStyle=grd; ctx.fillRect(0,0,width,height);
            ctx.beginPath(); ctx.moveTo(dragStartX,dragStartY); ctx.lineTo(dragEndX,dragEndY);
            ctx.strokeStyle="#fff"; ctx.lineWidth=2; ctx.setLineDash([6,4]); ctx.stroke(); ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(dragStartX,dragStartY,5,0,2*Math.PI); ctx.fillStyle="#fff"; ctx.fill();
            ctx.beginPath(); ctx.arc(dragEndX,dragEndY,5,0,2*Math.PI); ctx.strokeStyle="#fff"; ctx.lineWidth=2; ctx.stroke();
        }

        // 5. Radial drag preview (tool 4)
        if (tool===4 && draggingTool) {
            const dx=dragEndX-dragStartX, dy=dragEndY-dragStartY, r=Math.sqrt(dx*dx+dy*dy);
            const grd2=ctx.createRadialGradient(dragStartX,dragStartY,0,dragStartX,dragStartY,Math.max(r,1));
            grd2.addColorStop(0,"rgba(255,255,255,0.35)"); grd2.addColorStop(0.65,"rgba(255,255,255,0.18)"); grd2.addColorStop(1,"rgba(255,255,255,0)");
            ctx.fillStyle=grd2; ctx.fillRect(0,0,width,height);
            ctx.beginPath(); ctx.arc(dragStartX,dragStartY,Math.max(r,1),0,2*Math.PI);
            ctx.strokeStyle="rgba(255,255,255,0.8)"; ctx.lineWidth=1.5; ctx.setLineDash([6,4]); ctx.stroke(); ctx.setLineDash([]);
            ctx.beginPath(); ctx.arc(dragStartX,dragStartY,4,0,2*Math.PI); ctx.fillStyle="#fff"; ctx.fill();
        }
    }

    // Push a stroke to history and request repaint (instant visual feedback, no file I/O)
    function drawLocalStroke(x, y, erase) {
        const sw = docCtrl ? (docCtrl.sourceWidth  || width)  : width;
        const sh = docCtrl ? (docCtrl.sourceHeight || height) : height;
        const r  = Math.max(1, brushRadius * Math.min(width/sw, height/sh));
        maskCanvas.strokeHistory.push({x:x, y:y, r:r, erase:erase});
        requestPaint();
    }

    onWidthChanged:  requestPaint()
    onHeightChanged: requestPaint()

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
        onEntered:  { maskCanvas.cursorInside=true;  maskCanvas.requestPaint(); }
        onExited:   {
            maskCanvas.cursorInside=false;
            if (painting) {
                painting=false;
                maskCanvas.strokeHistory=[];
                if (docCtrl) docCtrl.commitMaskPaint();
            }
            maskCanvas.requestPaint();
        }
        onPressed: (mouse) => {
            if (!docCtrl) return;
            const tool = docCtrl.activeTool;
            // Brush/erase require paintEnabled; gradient/radial do not
            if ((tool===1||tool===2) && !paintEnabled) return;
            if (tool===1||tool===2) {
                painting=true;
                maskCanvas.drawLocalStroke(mouse.x, mouse.y, tool===2);
                doStroke(mouse.x, mouse.y, tool===2);
            } else if (tool===3||tool===4) {
                maskCanvas.dragStartX=mouse.x; maskCanvas.dragStartY=mouse.y;
                maskCanvas.dragEndX=mouse.x;   maskCanvas.dragEndY=mouse.y;
                maskCanvas.draggingTool=true;   maskCanvas.requestPaint();
            }
        }
        onReleased: (mouse) => {
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting) {
                painting=false;
                maskCanvas.strokeHistory=[];
                if (docCtrl) docCtrl.commitMaskPaint();
                // commitMaskPaint emits maskChanged → onMaskChanged → reload canvas
            }
            if (maskCanvas.draggingTool && docCtrl) {
                maskCanvas.draggingTool=false;
                const sw=docCtrl.sourceWidth||maskCanvas.width, sh=docCtrl.sourceHeight||maskCanvas.height;
                const x1=maskCanvas.dragStartX/maskCanvas.width*sw, y1=maskCanvas.dragStartY/maskCanvas.height*sh;
                const x2=maskCanvas.dragEndX/maskCanvas.width*sw,   y2=maskCanvas.dragEndY/maskCanvas.height*sh;
                if (tool===3) { docCtrl.applyGradientMask(x1,y1,x2,y2); }
                else if (tool===4) { const dx=x2-x1,dy=y2-y1; docCtrl.applyRadialMask(x1,y1,Math.sqrt(dx*dx+dy*dy)); }
                maskCanvas.requestPaint();
            }
        }
        onPositionChanged: (mouse) => {
            maskCanvas.cursorX=mouse.x; maskCanvas.cursorY=mouse.y;
            const tool = docCtrl ? docCtrl.activeTool : 0;
            if (painting && (tool===1||tool===2)) {
                maskCanvas.drawLocalStroke(mouse.x, mouse.y, tool===2);
                doStroke(mouse.x, mouse.y, tool===2);
            } else if (maskCanvas.draggingTool) {
                maskCanvas.dragEndX=mouse.x; maskCanvas.dragEndY=mouse.y; maskCanvas.requestPaint();
            } else { maskCanvas.requestPaint(); }
        }
        function doStroke(x, y, erase) {
            const sw=docCtrl.sourceWidth||maskCanvas.width, sh=docCtrl.sourceHeight||maskCanvas.height;
            docCtrl.paintMaskStroke(x/maskCanvas.width*sw, y/maskCanvas.height*sh, brushRadius, erase);
        }
    }
}
