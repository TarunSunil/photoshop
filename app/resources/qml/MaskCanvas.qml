import QtQuick
import QtQuick.Controls
Canvas {
    id: maskCanvas
    property var   docCtrl:        null
    property double brushRadius:   50
    property bool   eraseMode:     false
    property bool   paintEnabled:  false
    implicitWidth:  200
    implicitHeight: 200
    Connections {
        target: docCtrl
        function onMaskChanged() { maskCanvas.requestPaint() }
    }
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        if (!docCtrl || !docCtrl.hasDocument || !docCtrl.hasMask) return;
        const url = docCtrl.maskUrl;
        if (!url || url.length === 0) return;
        ctx.globalAlpha = 0.45;
        ctx.drawImage(url, 0, 0, width, height);
    }
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
