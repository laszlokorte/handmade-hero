import MetalKit
import UIKit

enum TouchState {
    case Begin
    case Move
    case Ended
}

class TouchMetalView: MTKView {
    var onTouch: ((TouchState, Int32, Int32) -> Void)?

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first {
            let p = t.location(in: self)  // points
            let scale = self.contentScaleFactor  // retina scaling

            let pixelX = p.x * scale
            let pixelY = p.y * scale
            onTouch?(.Begin, Int32(pixelX), Int32(pixelY))
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first {
            let p = t.location(in: self)  // points
            let scale = self.contentScaleFactor  // retina scaling

            let pixelX = p.x * scale
            let pixelY = p.y * scale
            onTouch?(.Move, Int32(pixelX), Int32(pixelY))
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first {
            let p = t.location(in: self)  // points
            let scale = self.contentScaleFactor  // retina scaling

            let pixelX = p.x * scale
            let pixelY = p.y * scale
            onTouch?(.Ended, Int32(pixelX), Int32(pixelY))
        }
    }

}
