import UIKit


final class CircleOverlayView: UIView {

    var onStick: (((Float, Float)?) -> Void)?
    private let baseLayer = CAShapeLayer()
    private let touchLayer = CAShapeLayer()

    private var baseRadius: CGFloat { min(bounds.width, bounds.height) * 0.5 }
    private var baseCenter: CGPoint { CGPoint(x: bounds.midX, y: bounds.midY) }



    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
        isUserInteractionEnabled = true

        baseLayer.lineWidth = 2
        baseLayer.fillColor = UIColor.white.withAlphaComponent(0.2).cgColor
        baseLayer.strokeColor = UIColor.white.withAlphaComponent(0.4).cgColor

        touchLayer.fillColor = UIColor.black.withAlphaComponent(0.5).cgColor
        touchLayer.isHidden = true

        layer.addSublayer(baseLayer)
        layer.addSublayer(touchLayer)
    }

    required init?(coder: NSCoder) { fatalError() }

    override func layoutSubviews() {
        super.layoutSubviews()

        baseLayer.path = UIBezierPath(
            arcCenter: baseCenter,
            radius: baseRadius,
            startAngle: 0,
            endAngle: .pi * 2,
            clockwise: true
        ).cgPath
    }

    private func updateTouchIndicator(at point: CGPoint) {
        let r: CGFloat = 28

        // Vector vom Zentrum zum Finger
        var dx = point.x - baseCenter.x
        var dy = point.y - baseCenter.y
        let distance = sqrt(dx*dx + dy*dy)


        onStick?((Float(dx / baseRadius), Float(dy / baseRadius)))
        // Clamp auf Radius des Basis-Kreises minus touchLayer radius
        let maxDistance = baseRadius
        if distance > maxDistance {
            let scale = maxDistance / distance
            dx *= scale
            dy *= scale
        }

        let clampedPoint = CGPoint(x: baseCenter.x + dx, y: baseCenter.y + dy)

        touchLayer.path = UIBezierPath(
            arcCenter: clampedPoint,
            radius: r,
            startAngle: 0,
            endAngle: .pi * 2,
            clockwise: true
        ).cgPath
    }

    // Touch Handling
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let t = touches.first else { return }
        touchLayer.isHidden = false
        onStick?(nil)
        updateTouchIndicator(at: t.location(in: self))
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let t = touches.first else { return }
        updateTouchIndicator(at: t.location(in: self))
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        touchLayer.isHidden = true
        onStick?(nil)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        touchLayer.isHidden = true

        onStick?(nil)
    }
}
