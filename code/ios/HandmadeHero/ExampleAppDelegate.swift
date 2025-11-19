import MetalKit
import UIKit

@main
class AppDelegate: UIResponder, UIApplicationDelegate, MTKViewDelegate {

    var window: UIWindow?
    var metalView: TouchMetalView!

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {

        window = UIWindow(frame: UIScreen.main.bounds)

        // Metal View als RootView
        metalView = TouchMetalView(frame: window!.bounds)
        metalView.onTouch = { (event: String, point: CGPoint) in

        }
        metalView.delegate = self
        metalView.clearColor = MTLClearColor(red: 1, green: 1, blue: 1, alpha: 1)
        metalView.device = MTLCreateSystemDefaultDevice()
        metalView.enableSetNeedsDisplay = true
        metalView.isPaused = false

        window?.rootViewController = UIViewController()
        window?.rootViewController?.view = metalView
        window?.makeKeyAndVisible()
        
        var GameInput = game_input();

        return true
    }
    func draw(in view: MTKView) {
        guard let drawable = view.currentDrawable,
            let passDescriptor = view.currentRenderPassDescriptor
        else { return }

        let commandBuffer = view.device!.makeCommandQueue()!.makeCommandBuffer()!
        let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: passDescriptor)!
        encoder.endEncoding()
        commandBuffer.present(drawable)
        commandBuffer.commit()
    }
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {

    }
}
