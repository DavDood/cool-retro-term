import QtQuick 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.1
import QtQuick.Window 2.2

ApplicationWindow {
    id: progressWindow

    width: 760
    height: 520
    minimumWidth: 520
    minimumHeight: 360
    visible: false
    title: renderController.renderTitle

    function progressValue() {
        if (renderController.totalFrames <= 0) {
            return 0
        }

        return Math.min(1, renderController.renderedFrames / renderController.totalFrames)
    }

    Component.onCompleted: renderController.start()

    Connections {
        target: renderController

        function onFinishedChanged() {
            if (!renderController.finished) {
                return
            }

            Qt.exit(renderController.failed ? 1 : 0)
        }
    }

    onClosing: appRoot.closeWindow(progressWindow)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Item {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 1
            visible: true
            clip: true
            opacity: 0.0

            RenderSurface {
                id: renderSurface
            }
        }

        Label {
            Layout.fillWidth: true
            text: renderController.renderTitle
            font.pixelSize: 20
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: renderController.statusText
            wrapMode: Text.WordWrap
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: progressWindow.progressValue()
            indeterminate: renderController.totalFrames <= 0 && renderController.running
        }

        Label {
            Layout.fillWidth: true
            text: renderController.totalFrames > 0
                ? qsTr("%1 / %2 frames rendered").arg(renderController.renderedFrames).arg(renderController.totalFrames)
                : qsTr("Preparing render pipeline...")
        }

        Label {
            Layout.fillWidth: true
            visible: renderController.warningsText.length > 0
            text: renderController.warningsText
            wrapMode: Text.WordWrap
            color: "#d08b00"
        }

        TextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            wrapMode: TextEdit.Wrap
            text: renderController.logText
        }
    }
}
