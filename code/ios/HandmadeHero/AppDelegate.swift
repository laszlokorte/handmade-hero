import MetalKit
import UIKit

@main
class AppDelegate: UIResponder, UIApplicationDelegate, MTKViewDelegate {

    var window: UIWindow?
    var metalView: TouchMetalView!

    var PlatformState: ios_state = ios_state()
    var GameMemory: game_memory = game_memory()

    var GameInputs = [game_input(), game_input()]

    var pipelineState: MTLRenderPipelineState?

    var commandQueue: MTLCommandQueue?

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {

        iOsSetupGameMemory(PlatformState: &PlatformState, GameMemory: &GameMemory)
        window = UIWindow(frame: UIScreen.main.bounds)

        // Metal View als RootView
        metalView = TouchMetalView(frame: window!.bounds)
        mtkView.drawableSize = mtkView.bounds.size
        let size = metalView.drawableSize
        PlatformState.RenderBuffer.Viewport.Width = UInt32(size.width)
        PlatformState.RenderBuffer.Viewport.Height = UInt32(size.height)
        metalView.autoResizeDrawable = true
        metalView.onTouch = { (event: TouchState, x: Int32, y: Int32) in
            switch event {
            case .Begin:
                self.GameInputs[0].Mouse.MouseX = x
                self.GameInputs[0].Mouse.MouseY = y
                self.GameInputs[0].Mouse.Buttons.0.EndedDown = true
                self.GameInputs[0].Mouse.Buttons.0.HalfTransitionCount += 1
                self.GameInputs[0].Mouse.InRange = true
                break
            case .Move:
                self.GameInputs[0].Mouse.MouseX = x
                self.GameInputs[0].Mouse.MouseY = y
                break
            case .Ended:
                self.GameInputs[0].Mouse.MouseX = x
                self.GameInputs[0].Mouse.MouseY = y
                self.GameInputs[0].Mouse.Buttons.0.EndedDown = false
                self.GameInputs[0].Mouse.Buttons.0.HalfTransitionCount += 1

                self.GameInputs[0].Mouse.InRange = false
                break
            }
        }
        metalView.delegate = self
        metalView.clearColor = MTLClearColor(red: 0, green: 1, blue: 1, alpha: 1)
        metalView.device = MTLCreateSystemDefaultDevice()
        metalView.enableSetNeedsDisplay = true
        metalView.isPaused = false
        metalView.framebufferOnly = true  // usually true
        metalView.sampleCount = 1

        guard let device = metalView.device else { return false }
        let library = try! device.makeDefaultLibrary(bundle: Bundle.main)
        let vertexFunction = library.makeFunction(name: "v_main")
        let fragmentFunction = library.makeFunction(name: "f_main")

        let pipelineDescriptor = MTLRenderPipelineDescriptor()
        pipelineDescriptor.vertexFunction = vertexFunction
        pipelineDescriptor.fragmentFunction = fragmentFunction
        pipelineDescriptor.colorAttachments[0].pixelFormat = metalView.colorPixelFormat
        if let colorAttachment = pipelineDescriptor.colorAttachments[0] {
            colorAttachment.isBlendingEnabled = true
            colorAttachment.rgbBlendOperation = .add
            colorAttachment.alphaBlendOperation = .add
            colorAttachment.sourceRGBBlendFactor = .sourceAlpha
            colorAttachment.sourceAlphaBlendFactor = .sourceAlpha
            colorAttachment.destinationRGBBlendFactor = .oneMinusSourceAlpha
            colorAttachment.destinationAlphaBlendFactor = .oneMinusSourceAlpha
        }
        pipelineState = try? device.makeRenderPipelineState(descriptor: pipelineDescriptor)
        commandQueue = metalView.device?.makeCommandQueue()
        window?.rootViewController = UIViewController()
        window?.rootViewController?.view = metalView
        window?.makeKeyAndVisible()

        return true
    }
    func draw(in view: MTKView) {
        guard let drawable = view.currentDrawable,
            let passDescriptor = view.currentRenderPassDescriptor
        else { return }

        passDescriptor.colorAttachments[0].clearColor = MTLClearColor(
            red: 1, green: 0, blue: 0, alpha: 1)
        passDescriptor.colorAttachments[0].loadAction = .clear
        passDescriptor.colorAttachments[0].storeAction = .store

        let commandBuffer = commandQueue!.makeCommandBuffer()!
        let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: passDescriptor)!
        encoder.endEncoding()
        commandBuffer.present(drawable)
        commandBuffer.commit()
    }
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
        PlatformState.RenderBuffer.Viewport.Width = UInt32(size.width)
        PlatformState.RenderBuffer.Viewport.Height = UInt32(size.height)
    }
}

struct ios_state {
    var Running: Bool = false

    var TotalMemorySize: size_t = 0
    var GameMemoryBlock: UnsafeMutableRawPointer? = nil
    var RenderBuffer: render_buffer = render_buffer()
    var WorkQueue: work_queue = work_queue()
    var ThreadPool: ios_thread_pool = ios_thread_pool()
    var DebugSoundWave: Bool = false
}

func iOsSetupGameMemory(PlatformState: inout ios_state, GameMemory: inout game_memory) {

    PlatformState.Running = true
    PlatformState.TotalMemorySize = 1024 * 1024 * 100
    PlatformState.GameMemoryBlock = UnsafeMutableRawPointer.allocate(
        byteCount: PlatformState.TotalMemorySize,
        alignment: MemoryLayout<UInt8>.alignment
    )
    PlatformState.RenderBuffer.Size = 1000
    PlatformState.RenderBuffer.Count = 0
    PlatformState.RenderBuffer.Base = UnsafeMutablePointer<render_command>.allocate(
        capacity: PlatformState.RenderBuffer.Size)
    PlatformState.WorkQueue.Size = 1000
    PlatformState.WorkQueue.NextWrite = 0
    PlatformState.WorkQueue.NextRead = 0
    PlatformState.WorkQueue.CompletionGoal = 0
    PlatformState.WorkQueue.CompletionCount = 0
    PlatformState.WorkQueue.Base = UnsafeMutablePointer<ios_work_queue_task>.allocate(
        capacity: PlatformState.WorkQueue.Size)

    GameMemory.Initialized = false
    GameMemory.PermanentStorageSize = 1024 * 1024
    GameMemory.PermanentStorage = UnsafeMutablePointer<uint8>.allocate(
        capacity: GameMemory.PermanentStorageSize)

    GameMemory.TransientStorageSize = 1024 * 1024
    GameMemory.TransientStorage = UnsafeMutablePointer<uint8>.allocate(
        capacity: GameMemory.TransientStorageSize
    )

    GameMemory.TaskQueue = withUnsafeMutablePointer(to: &PlatformState.WorkQueue) { p in
        UnsafeMutablePointer(p)
    }
    GameMemory.DebugPlatformFreeFileMemory = iOSDebugPlatformFreeFileMemory
    GameMemory.DebugPlatformReadEntireFile = iOSDebugPlatformReadEntireFile
    GameMemory.DebugPlatformWriteEntireFile = iOSDebugPlatformWriteEntireFile

    GameMemory.PlatformPushTaskToQueue = iOSPlatformPushTaskToQueue
    GameMemory.PlatformWaitForQueueToFinish = iOSPlatformWaitForQueueToFinish

}
