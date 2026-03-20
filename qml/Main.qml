import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.layershell as LayerShell

Window {
    id: root
    flags: Qt.FramelessWindowHint
    color: "transparent"
    width: Math.min(Screen.width * 0.48, 740)
    height: Screen.height
    visible: false

    LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft
    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.scope: "rog-ally-menu"
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityOnDemand

    readonly property color bgPrimary: "#000000"
    readonly property color bgCard: "#1a1a1e"
    readonly property color bgCardHover: "#252528"
    readonly property color bgCardSelected: "#0a2830"
    readonly property color accent: "#ff0033"
    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: "#777777"
    readonly property color textIcon: "#888888"
    readonly property color selectedBorder: "#00bcd4"

    property bool showing: false
    property int selectedTile: 6


    function toggle() {
        if (showing) {
            backend.stopPolling()
            showing = false
            hideTimer.start()
        } else {
            visible = true
            backend.refreshAll()
            backend.startPolling()
            showing = true
        }
    }

    function formatProfile(p) {
        if (p === "low-power") return "Silent"
        if (p === "balanced") return "Balanced"
        if (p === "performance") return "Performance"
        return p
    }

    Timer {
        id: hideTimer
        interval: 260
        onTriggered: if (!showing) root.visible = false
    }



    // Gamepad/keyboard navigation
    function activateTile(idx) {
        if (idx === 0) backend.cycleOperatingMode()
        else if (idx === 2) backend.endTask()
        else if (idx === 3) {
            var next = backend.ledBrightness < 10 ? 10 : (backend.ledBrightness < 100 ? 100 : 0)
            backend.cycleLedBrightness(next)
        }
        else if (idx === 4) backend.cycleLedColor()
        else if (idx === 5) backend.toggleAirplaneMode()
        else if (idx === 6) backend.takeScreenshot()
        else if (idx === 7) backend.showKeyboard()
        else if (idx === 8) { backend.showDesktop(); toggle() }
    }

    Item {
        focus: showing
        anchors.fill: parent
        Keys.onRightPressed: { selectedTile = Math.min(8, selectedTile + 1) }
        Keys.onLeftPressed: { selectedTile = Math.max(0, selectedTile - 1) }
        Keys.onDownPressed: { selectedTile = Math.min(8, selectedTile + 3) }
        Keys.onUpPressed: { selectedTile = Math.max(0, selectedTile - 3) }
        Keys.onReturnPressed: { activateTile(selectedTile) }
        Keys.onEnterPressed: { activateTile(selectedTile) }
        Keys.onEscapePressed: { toggle() }
    }

    Rectangle {
        anchors.fill: parent
        color: bgPrimary

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ---- Left Sidebar with Vertical Sliders ----
            Item {
                Layout.preferredWidth: 56
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 50
                    anchors.bottomMargin: 20
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 4

                    VSlider {
                        value: backend.screenBrightness
                        from: 0; to: 100
                        onMoved: function(v) { backend.screenBrightness = v }
                    }
                    // Sun icon for brightness
                    Text {
                        text: "\u2600"
                        color: textSecondary
                        font.pixelSize: 18
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Item { Layout.fillHeight: true }

                    VSlider {
                        value: backend.volume
                        from: 0; to: 100
                        onMoved: function(v) { backend.volume = v }
                    }
                    // Speaker icon for volume
                    Canvas {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 18
                        Layout.alignment: Qt.AlignHCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.fillStyle = "#777777"
                            ctx.strokeStyle = "#777777"
                            ctx.lineWidth = 1.5
                            // Speaker body
                            ctx.fillRect(0, 6, 5, 6)
                            ctx.beginPath()
                            ctx.moveTo(5, 6)
                            ctx.lineTo(11, 1)
                            ctx.lineTo(11, 17)
                            ctx.lineTo(5, 12)
                            ctx.closePath()
                            ctx.fill()
                            // Sound waves
                            ctx.beginPath()
                            ctx.arc(12, 9, 4, -0.8, 0.8)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.arc(12, 9, 7.5, -0.7, 0.7)
                            ctx.stroke()
                        }
                    }

                    Item { Layout.preferredHeight: 16 }

                    // Power button
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignHCenter
                        radius: 16
                        color: "transparent"
                        border.color: textSecondary
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "\u23FB"
                            color: textSecondary
                            font.pixelSize: 16
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.suspend()
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: "#222222"
            }

            // ---- Main Content ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                spacing: 20

                // ---- Header ----
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Command Center"
                        color: textPrimary
                        font { pixelSize: 22; weight: Font.Bold }
                    }
                    Item { Layout.fillWidth: true }

                    Row {
                        spacing: 10
                        layoutDirection: Qt.LeftToRight

                        // Bluetooth logo
                        Canvas {
                            width: 12; height: 16
                            anchors.verticalCenter: parent.verticalCenter
                            onPaint: {
                                var ctx = getContext("2d")
                                var c = backend.bluetoothConnected ? "#ffffff" : "#555555"
                                ctx.strokeStyle = c
                                ctx.lineWidth = 1.5
                                ctx.lineCap = "round"
                                ctx.beginPath()
                                // Main vertical line
                                ctx.moveTo(6, 0)
                                ctx.lineTo(6, 16)
                                // Top arrow
                                ctx.moveTo(6, 0)
                                ctx.lineTo(10, 4)
                                ctx.lineTo(2, 12)
                                // Bottom arrow
                                ctx.moveTo(6, 16)
                                ctx.lineTo(10, 12)
                                ctx.lineTo(2, 4)
                                ctx.stroke()
                            }
                        }

                        // Wifi icon (3 arcs)
                        Item {
                            width: 16; height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    var c = backend.wifiConnected ? "#ffffff" : "#555555"
                                    ctx.strokeStyle = c
                                    ctx.lineWidth = 1.5
                                    for (var i = 0; i < 3; i++) {
                                        ctx.beginPath()
                                        ctx.arc(8, 14, 4 + i * 4, -Math.PI * 0.8, -Math.PI * 0.2)
                                        ctx.stroke()
                                    }
                                    ctx.fillStyle = c
                                    ctx.beginPath()
                                    ctx.arc(8, 14, 2, 0, Math.PI * 2)
                                    ctx.fill()
                                }
                            }
                        }

                        Text {
                            text: backend.batteryPercent + "%"
                            color: textPrimary
                            font { pixelSize: 13; weight: Font.DemiBold }
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // Battery icon
                        Item {
                            width: 26; height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle {
                                x: 0; y: 1; width: 22; height: 12; radius: 2
                                color: "transparent"
                                border.color: textPrimary; border.width: 1
                                Rectangle {
                                    x: 2; y: 2
                                    width: Math.max(0, (parent.width - 4) * backend.batteryPercent / 100)
                                    height: parent.height - 4
                                    color: backend.batteryPercent <= 20 ? accent : textPrimary
                                    radius: 1
                                }
                            }
                            Rectangle {
                                x: 22; y: 4; width: 3; height: 6
                                radius: 1; color: textPrimary
                            }
                        }

                        Text {
                            text: backend.batteryWatts.toFixed(1) + "W"
                            color: textSecondary
                            font { pixelSize: 12 }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: backend.currentTime
                            color: textPrimary
                            font { pixelSize: 13 }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Close button
                    Rectangle {
                        width: 28; height: 28; radius: 14
                        color: closeMA.containsMouse ? "#333333" : "transparent"
                        Layout.alignment: Qt.AlignVCenter
                        Text {
                            anchors.centerIn: parent
                            text: "\u2715"
                            color: closeMA.containsMouse ? accent : textSecondary
                            font.pixelSize: 16
                        }
                        MouseArea {
                            id: closeMA
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: toggle()
                        }
                    }
                }

                // ---- Tile Grid ----
                GridLayout {
                    Layout.fillWidth: true
                    Layout.maximumHeight: Math.min(Screen.height * 0.65, 560)
                    columns: 3
                    rowSpacing: 12
                    columnSpacing: 12

                    // === Row 1 ===

                    // Operating Mode
                    CmdTile {
                        selected: selectedTile === 0
                        onClicked: { backend.cycleOperatingMode(); selectedTile = 0 }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 54; height: 46; radius: 6
                            color: "transparent"
                            border.color: textIcon; border.width: 1.5
                            Text {
                                anchors.centerIn: parent
                                text: backend.tdpSpl + "W"
                                color: accent
                                font { pixelSize: 15; weight: Font.Bold }
                            }
                        }
                        Text {
                            text: formatProfile(backend.profile)
                            color: accent
                            font { pixelSize: 13; weight: Font.DemiBold }
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "Operating Mode"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // Control Mode
                    CmdTile {
                        selected: selectedTile === 1
                        onClicked: { selectedTile = 1 }

                        // Handheld device icon
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: 58; height: 42
                            Rectangle {
                                anchors.centerIn: parent
                                width: 56; height: 36; radius: 12
                                color: "transparent"
                                border.color: textIcon; border.width: 1.5
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 24; height: 20; radius: 3
                                    color: "transparent"
                                    border.color: textIcon; border.width: 1
                                }
                            }
                        }
                        Text {
                            text: "Auto"
                            color: accent
                            font { pixelSize: 13; weight: Font.DemiBold }
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "Control Mode"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // End Task
                    CmdTile {
                        selected: selectedTile === 2
                        onClicked: { backend.endTask(); selectedTile = 2 }

                        Row {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 3
                            Rectangle {
                                width: 30; height: 30; radius: 4
                                color: "transparent"
                                border.color: textIcon; border.width: 1
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0
                                    Text { text: "ALT"; color: textIcon; font.pixelSize: 7; font.weight: Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
                                    Text { text: "F4"; color: textIcon; font.pixelSize: 9; font.weight: Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
                                }
                            }
                            Text {
                                text: "\u2715"
                                color: textIcon
                                font { pixelSize: 30; weight: Font.Bold }
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Text {
                            text: "End Task"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // === Row 2 ===

                    // LED Brightness
                    CmdTile {
                        selected: selectedTile === 3
                        onClicked: {
                            selectedTile = 3
                            var next = 10
                            if (backend.ledBrightness < 10) next = 10
                            else if (backend.ledBrightness < 100) next = 100
                            else next = 0
                            backend.cycleLedBrightness(next)
                        }

                        // Gauge arc with dots
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: 56; height: 38
                            Repeater {
                                model: 9
                                Rectangle {
                                    required property int index
                                    width: 4; height: 4; radius: 2
                                    color: textIcon
                                    x: 28 + 24 * Math.cos(Math.PI + index * Math.PI / 8) - 2
                                    y: 34 + 24 * Math.sin(Math.PI + index * Math.PI / 8) - 2
                                }
                            }
                        }
                        Text {
                            text: backend.ledBrightness + "%"
                            color: accent
                            font { pixelSize: 13; weight: Font.DemiBold }
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "LED brightness"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // LED Color
                    CmdTile {
                        selected: selectedTile === 4
                        onClicked: { backend.cycleLedColor(); selectedTile = 4 }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 36; height: 36; radius: 18
                            color: backend.ledColor
                            border.color: textIcon; border.width: 1.5
                        }
                        Text {
                            text: {
                                var r = backend.ledColor.r * 255, g = backend.ledColor.g * 255, b = backend.ledColor.b * 255
                                if (r === 139 && g === 0 && b === 0) return "Dark Red"
                                if (r === 255 && g === 0 && b === 0) return "Red"
                                if (r === 0 && g === 0 && b === 255) return "Blue"
                                if (r === 0 && g === 255 && b === 0) return "Green"
                                if (r === 255 && g === 165 && b === 0) return "Orange"
                                if (r === 255 && g === 255 && b === 255) return "White"
                                return "Custom"
                            }
                            color: accent
                            font { pixelSize: 13; weight: Font.DemiBold }
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "LED Color"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // Airplane Mode
                    CmdTile {
                        selected: selectedTile === 5
                        onClicked: { backend.toggleAirplaneMode(); selectedTile = 5 }

                        // Airplane icon
                        Canvas {
                            Layout.alignment: Qt.AlignHCenter
                            width: 44; height: 44
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.fillStyle = textIcon
                                ctx.beginPath()
                                // Fuselage
                                ctx.ellipse(18, 6, 8, 32)
                                ctx.fill()
                                // Wings
                                ctx.beginPath()
                                ctx.moveTo(22, 16)
                                ctx.lineTo(42, 22)
                                ctx.lineTo(42, 24)
                                ctx.lineTo(22, 22)
                                ctx.fill()
                                ctx.beginPath()
                                ctx.moveTo(22, 16)
                                ctx.lineTo(2, 22)
                                ctx.lineTo(2, 24)
                                ctx.lineTo(22, 22)
                                ctx.fill()
                                // Tail
                                ctx.beginPath()
                                ctx.moveTo(22, 32)
                                ctx.lineTo(30, 36)
                                ctx.lineTo(30, 37)
                                ctx.lineTo(22, 35)
                                ctx.fill()
                                ctx.beginPath()
                                ctx.moveTo(22, 32)
                                ctx.lineTo(14, 36)
                                ctx.lineTo(14, 37)
                                ctx.lineTo(22, 35)
                                ctx.fill()
                            }
                        }
                        Text {
                            text: backend.airplaneMode ? "On" : "Off"
                            color: accent
                            font { pixelSize: 13; weight: Font.DemiBold }
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "Airplane Mode"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // === Row 3 ===

                    // Take Screenshot
                    CmdTile {
                        selected: selectedTile === 6
                        onClicked: { backend.takeScreenshot(); selectedTile = 6 }

                        // Camera with corner brackets
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: 50; height: 42
                            // Corner brackets
                            Rectangle { x: 0; y: 0; width: 10; height: 2; color: textIcon }
                            Rectangle { x: 0; y: 0; width: 2; height: 10; color: textIcon }
                            Rectangle { x: 40; y: 0; width: 10; height: 2; color: textIcon }
                            Rectangle { x: 48; y: 0; width: 2; height: 10; color: textIcon }
                            Rectangle { x: 0; y: 40; width: 10; height: 2; color: textIcon }
                            Rectangle { x: 0; y: 32; width: 2; height: 10; color: textIcon }
                            Rectangle { x: 40; y: 40; width: 10; height: 2; color: textIcon }
                            Rectangle { x: 48; y: 32; width: 2; height: 10; color: textIcon }
                            // Camera body
                            Rectangle {
                                anchors.centerIn: parent
                                width: 30; height: 22; radius: 3
                                color: "transparent"
                                border.color: textIcon; border.width: 1.5
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 12; height: 12; radius: 6
                                    color: "transparent"
                                    border.color: textIcon; border.width: 1.5
                                }
                            }
                        }
                        Text {
                            text: "Take Screenshot"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // Show Keyboard
                    CmdTile {
                        selected: selectedTile === 7
                        onClicked: { backend.showKeyboard(); selectedTile = 7 }

                        // Keyboard icon
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 50; height: 34; radius: 4
                            color: "transparent"
                            border.color: textIcon; border.width: 1.5
                            Column {
                                anchors.centerIn: parent
                                spacing: 3
                                Row {
                                    spacing: 3; anchors.horizontalCenter: parent.horizontalCenter
                                    Repeater { model: 5; Rectangle { width: 6; height: 5; radius: 1; color: textIcon } }
                                }
                                Row {
                                    spacing: 3; anchors.horizontalCenter: parent.horizontalCenter
                                    Repeater { model: 4; Rectangle { width: 6; height: 5; radius: 1; color: textIcon } }
                                }
                                Rectangle { width: 26; height: 4; radius: 1; color: textIcon; anchors.horizontalCenter: parent.horizontalCenter }
                            }
                        }
                        Text {
                            text: "Show Keyboard"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    // Show Desktop
                    CmdTile {
                        selected: selectedTile === 8
                        onClicked: { backend.showDesktop(); toggle() }

                        // Desktop/window icon
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: 50; height: 40
                            // Back window
                            Rectangle {
                                x: 10; y: 0; width: 36; height: 28; radius: 3
                                color: "transparent"
                                border.color: textIcon; border.width: 1
                            }
                            // Front window
                            Rectangle {
                                x: 4; y: 10; width: 36; height: 28; radius: 3
                                color: bgCard
                                border.color: textIcon; border.width: 1.5
                                Rectangle {
                                    x: 2; y: 2; width: parent.width - 4; height: 7
                                    color: textIcon; opacity: 0.3; radius: 1
                                }
                            }
                        }
                        Text {
                            text: "Show Desktop"
                            color: textPrimary; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    // ---- Inline Components ----

    component CmdTile: Item {
        default property alias content: contentCol.data
        property bool selected: false
        signal clicked()

        Layout.fillWidth: true
        Layout.fillHeight: true

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: selected ? bgCardSelected : (tileMA.containsMouse ? bgCardHover : bgCard)
            border.color: selected ? selectedBorder : "transparent"
            border.width: selected ? 2 : 0

            // Triangle indicator top-left
            Canvas {
                width: 8; height: 8
                anchors { left: parent.left; top: parent.top; leftMargin: 10; topMargin: 10 }
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.fillStyle = "#555555"
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(width, 0)
                    ctx.lineTo(0, height)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        ColumnLayout {
            id: contentCol
            anchors.centerIn: parent
            spacing: 6
        }

        MouseArea {
            id: tileMA
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    component VSlider: Item {
        property real value: 0
        property real from: 0
        property real to: 100
        property var onMoved: null

        Layout.fillHeight: true
        Layout.preferredWidth: 40
        Layout.alignment: Qt.AlignHCenter

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 6; height: parent.height
            y: 0

            Rectangle {
                anchors.fill: parent
                radius: 3
                color: "#222222"
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height > 0 ? parent.height * Math.max(0, Math.min(1, (value - from) / Math.max(1, to - from))) : 0
                radius: 3
                color: accent
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: {
                    var ratio = Math.max(0, Math.min(1, (value - from) / Math.max(1, to - from)))
                    return parent.height * (1.0 - ratio) - 7
                }
                width: 14; height: 14; radius: 7
                color: "#cccccc"
                border.color: "#888888"; border.width: 1
            }
        }

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onPressed: function(mouse) { handleDrag(mouse.y) }
            onPositionChanged: function(mouse) { if (pressed) handleDrag(mouse.y) }
            function handleDrag(mouseY) {
                var ratio = 1.0 - mouseY / height
                ratio = Math.max(0, Math.min(1, ratio))
                var newVal = Math.round(from + ratio * (to - from))
                if (onMoved) onMoved(newVal)
            }
        }
    }

    component SidebarIcon: Item {
        property alias iconChar: lbl.text
        Layout.preferredHeight: 28
        Layout.preferredWidth: 28
        Layout.alignment: Qt.AlignHCenter
        Text {
            id: lbl
            anchors.centerIn: parent
            color: textSecondary
            font.pixelSize: 18
        }
    }
}
