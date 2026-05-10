import QtQuick 2.2
import Qt5Compat.GraphicalEffects

Item {
    id: renderFrameView

    property string pendingOutputPath: ""
    property int pendingGrabFrames: 0
    property real normalizedWindowScale: 1.0
    readonly property real captureDevicePixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
    readonly property size captureSize: Qt.size(
        Math.max(1, Math.round(renderFrameView.width / captureDevicePixelRatio)),
        Math.max(1, Math.round(renderFrameView.height / captureDevicePixelRatio))
    )

    width: Math.max(1, renderController.frameWidth)
    height: Math.max(1, renderController.frameHeight)
    visible: renderController.running || renderController.finished
    x: 0
    y: 0
    z: -1
    opacity: 0.01

    function scheduleCapture() {
        if (!pendingOutputPath || frameImage.status !== Image.Ready) {
            return
        }

        pendingGrabFrames = 2
        if (!grabDriver.running) {
            grabDriver.start()
        }
    }

    Connections {
        target: renderController

        onCurrentFrameSourceChanged: {
            pendingOutputPath = renderController.currentFrameOutput
            frameImage.source = ""
            frameImage.source = renderController.currentFrameSource
            renderFrameView.scheduleCapture()
        }
    }

    Timer {
        id: grabDriver
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (pendingGrabFrames > 0) {
                pendingGrabFrames -= 1
                return
            }

            running = false
            renderedShader.grabToImage(function(finalResult) {
                const finalSaved = finalResult && finalResult.saveToFile(pendingOutputPath)
                pendingOutputPath = ""
                renderController.completeCurrentFrame(finalSaved)
            }, captureSize)
        }
    }

    Item {
        id: frameContent
        anchors.fill: parent

        Image {
            id: frameImage
            anchors.fill: parent
            asynchronous: false
            cache: false
            fillMode: Image.Stretch
            smooth: true
            sourceSize.width: renderFrameView.width
            sourceSize.height: renderFrameView.height
            onStatusChanged: {
                if (status === Image.Error && pendingOutputPath) {
                    const failedPath = pendingOutputPath
                    pendingOutputPath = ""
                    renderController.completeCurrentFrame(false)
                } else if (status === Image.Ready) {
                    frameSourceTexture.scheduleUpdate()
                    burnInEffect.sourceUpdated()
                    renderFrameView.scheduleCapture()
                }
            }
        }
    }

    ShaderEffectSource {
        id: frameSourceTexture
        sourceItem: frameContent
        live: true
        hideSource: true
        wrapMode: ShaderEffectSource.Repeat
        visible: false
    }

    RenderBurnInEffect {
        id: burnInEffect
        anchors.fill: parent
        contentSource: frameSourceTexture
    }

    Loader {
        id: bloomEffectLoader
        active: appSettings.bloom > 0 || appSettings._frameShininess > 0
        asynchronous: true
        width: parent.width * appSettings.bloomQuality
        height: parent.height * appSettings.bloomQuality

        sourceComponent: FastBlur {
            radius: 16 + 48 * appSettings.bloomQuality
            source: frameSourceTexture
            transparentBorder: true
        }
    }

    Loader {
        id: bloomSourceLoader
        active: bloomEffectLoader.active
        asynchronous: true
        sourceComponent: ShaderEffectSource {
            sourceItem: bloomEffectLoader.item
            wrapMode: ShaderEffectSource.Repeat
            hideSource: true
            smooth: true
            visible: false
        }
    }

    ShaderTerminal {
        id: renderedShader
        anchors.fill: parent
        source: frameSourceTexture
        burnInEffect: burnInEffect
        normalizedWindowScale: renderFrameView.normalizedWindowScale
        virtualResolution: Qt.size(renderFrameView.width, renderFrameView.height)
        screenResolution: Qt.size(renderFrameView.width, renderFrameView.height)
        bloomSource: bloomSourceLoader.item
    }
}
