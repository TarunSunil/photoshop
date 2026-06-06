import QtQuick
import QtQuick.Controls

Canvas {
    id: maskCanvas
    property DocumentController documentController
    property double brushRadius: 50
    property bool eraseMode: false
    property bool activeTool: false

    implicitWidth: 200
    implicitHeight: 200

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();

        if (!documentController.hasDocument || !documentController.activeMask) {
            return;
        }

        // Draw the mask overlay with semi-transparent red
        const maskImage = documentController.activeMask;
        if (maskImage) {
            ctx.globalAlpha = 0.4;
            ctx.fillStyle = "#ff4444";
            ctx.drawImage(maskImage, 0, 0);
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true

        property bool isDragging: false

        onPressed: (mouse) => {
            if (activeTool) {
                isDragging = true;
                paintStroke(mouse.x, mouse.y);
            }
        }

        onReleased: {
            isDragging = false;
        }

        onExited: {
            isDragging = false;
        }

        onPositionChanged: (mouse) => {
            if (isDragging && activeTool) {
                paintStroke(mouse.x, mouse.y);
            }
        }

        function paintStroke(x, y) {
            documentController.paintMaskStroke(x, y, brushRadius, eraseMode);
            maskCanvas.requestPaint();
        }
    }
}
