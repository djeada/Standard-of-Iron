import QtQuick 2.15

Item {
    id: cursorManager
    
    property string currentMode: "normal"
    property var cursorItem: null
    
    
    Component {
        id: attackCursorComponent
        Canvas {
            width: 32
            height: 32
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                
                ctx.strokeStyle = "#e74c3c"
                ctx.lineWidth = 2
                
                
                ctx.beginPath()
                ctx.moveTo(16, 4)
                ctx.lineTo(16, 28)
                ctx.stroke()
                
                
                ctx.beginPath()
                ctx.moveTo(4, 16)
                ctx.lineTo(28, 16)
                ctx.stroke()
                
                
                ctx.fillStyle = "#e74c3c"
                ctx.beginPath()
                ctx.arc(16, 16, 3, 0, Math.PI * 2)
                ctx.fill()
                
                
                ctx.strokeStyle = "#c0392b"
                ctx.lineWidth = 2
                
                
                ctx.beginPath()
                ctx.moveTo(8, 12)
                ctx.lineTo(8, 8)
                ctx.lineTo(12, 8)
                ctx.stroke()
                
                
                ctx.beginPath()
                ctx.moveTo(20, 8)
                ctx.lineTo(24, 8)
                ctx.lineTo(24, 12)
                ctx.stroke()
                
                
                ctx.beginPath()
                ctx.moveTo(8, 20)
                ctx.lineTo(8, 24)
                ctx.lineTo(12, 24)
                ctx.stroke()
                
                
                ctx.beginPath()
                ctx.moveTo(20, 24)
                ctx.lineTo(24, 24)
                ctx.lineTo(24, 20)
                ctx.stroke()
            }
        }
    }
    
    Component {
        id: guardCursorComponent
        Canvas {
            width: 32
            height: 32
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                
                ctx.fillStyle = "#3498db"
                ctx.strokeStyle = "#2980b9"
                ctx.lineWidth = 2
                
                
                ctx.beginPath()
                ctx.moveTo(16, 6)
                ctx.lineTo(24, 10)
                ctx.lineTo(24, 18)
                ctx.lineTo(16, 26)
                ctx.lineTo(8, 18)
                ctx.lineTo(8, 10)
                ctx.closePath()
                ctx.fill()
                ctx.stroke()
                
                
                ctx.strokeStyle = "#ecf0f1"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(13, 16)
                ctx.lineTo(15, 18)
                ctx.lineTo(19, 12)
                ctx.stroke()
            }
        }
    }
    
    Component {
        id: patrolCursorComponent
        Canvas {
            width: 32
            height: 32
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                
                ctx.fillStyle = "#27ae60"
                ctx.strokeStyle = "#229954"
                ctx.lineWidth = 2
                
                
                ctx.beginPath()
                ctx.arc(16, 16, 10, 0, Math.PI * 2)
                ctx.stroke()
                
                
                ctx.fillStyle = "#27ae60"
                
                
                ctx.beginPath()
                ctx.moveTo(26, 16)
                ctx.lineTo(22, 13)
                ctx.lineTo(22, 19)
                ctx.closePath()
                ctx.fill()
                
                
                ctx.beginPath()
                ctx.moveTo(6, 16)
                ctx.lineTo(10, 13)
                ctx.lineTo(10, 19)
                ctx.closePath()
                ctx.fill()
                
                
                ctx.beginPath()
                ctx.arc(16, 16, 3, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }
    
    function updateCursor(mode) {
        currentMode = mode
        
        
        if (cursorItem) {
            cursorItem.destroy()
            cursorItem = null
        }
        
        
        switch(mode) {
            case "attack":
                cursorItem = attackCursorComponent.createObject(cursorManager)
                break
            case "guard":
                cursorItem = guardCursorComponent.createObject(cursorManager)
                break
            case "patrol":
                cursorItem = patrolCursorComponent.createObject(cursorManager)
                break
            default:
                
                break
        }
    }
}
