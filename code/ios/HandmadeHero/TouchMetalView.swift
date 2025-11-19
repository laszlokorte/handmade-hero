import MetalKit
import UIKit

class TouchMetalView: MTKView {
    var onTouch: ((String, CGPoint) -> Void)?

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { onTouch?("began", t.location(in: self)) }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { onTouch?("moved", t.location(in: self)) }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { onTouch?("ended", t.location(in: self)) }
    }
}
