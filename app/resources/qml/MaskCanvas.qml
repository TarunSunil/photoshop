import QtQuick
import QtQuick.Controls
Canvas {
    id: maskCanvas
    property var   docCtrl:        null
    property double brushRadius:   50
    property bool   eraseMode:     false
    property bool   paintEnabled:  false
    // Track which mask url is currently loaded into the Canvas image cache.
    // Context2D.drawImage(urlString, ...) silently draws nothing unless
    // that exact url was previously passed to Canvas.loadImage() and has
    // finished loading (Qt docs: "Only loaded images can be painted on
    // the Canvas item"). The old code called ctx.drawImage(url, ...)
    // directly with no preload step at all, so the brush/mask overlay
    // never appeared no matter what was painted.
    property string loadedUrl: ""
    implicitWidth:  200
    implicitHeight: 200
    Connections {
        target: docCtrl
        function onMaskChanged() {
            const url = docCtrl && docCtrl.hasMask ? docCtrl.maskUrl : "";
            if (maskCanvas.loadedUrl.length > 0 && maskCanvas.loadedUrl !== url)
                maskCanvas.unloadImage(maskCanvas.loadedUrl);
            if (url.length === 0) {
                maskCanvas.loadedUrl = "";
                maskCanvas.requestPaint();
                return;
            }
            // Each brush stroke writes a new temp file
            // (lumenforge-mask-N.png), so the url changes on every
            // stroke and has to be (re)loaded every time, not just once.
            maskCanvas.loadImage(url);
        }
    }
    onImageLoaded: {
        loadedUrl = docCtrl ? docCtrl.maskUrl : "";
        requestPaint();
    }
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        if (!docCtrl || !docCtrl.hasDocument || !docCtrl.hasMask) return;
        const url = docCtrl.maskUrl;
        if (!url || url.length === 0) return;
        if (!isImageLoaded(url)) {
            // Not ready yet (or load hasn't been kicked off this paint
            // cycle) — request the load and bail; onImageLoaded will
            // call requestPaint() again once it's actually available.
            loadImage(url);
            return;
        }
        ctx.globalAlpha = 0.45;
        ctx.drawImage(url, 0, 0, width, height);
    }
    onWidthChanged:  requestPaint()
    onHeightChanged: requestPaint()
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property bool dragging: false
        onPressed:  (mouse) => { if (paintEnabled) { dragging = true;  stroke(mouse.x, mouse.y) } }
        onReleased:            { dragging = false }
        onExited:              { dragging = false }
        onPositionChanged: (mouse) => { if (dragging && paintEnabled) stroke(mouse.x, mouse.y) }
        function stroke(x, y) {
            const sw = docCtrl.sourceWidth  || maskCanvas.width
            const sh = docCtrl.sourceHeight || maskCanvas.height
            docCtrl.paintMaskStroke(
                x / maskCanvas.width  * sw,
                y / maskCanvas.height * sh,
                brushRadius, eraseMode)
        }
    }
}
