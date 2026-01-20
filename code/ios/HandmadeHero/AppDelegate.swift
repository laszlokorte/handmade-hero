import MetalKit
import UIKit

@main
class AppDelegate: UIResponder, UIApplicationDelegate, MTKViewDelegate {

    var window: UIWindow?
    var metalView: TouchMetalView!

    var PlatformState: ios_state = ios_state()
    var GameMemory: game_memory = game_memory()

    var GameInput = game_input()

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
        let size = metalView.drawableSize
        PlatformState.RenderBuffer.Viewport.Width = UInt32(size.width)
        PlatformState.RenderBuffer.Viewport.Height = UInt32(size.height)
        metalView.autoResizeDrawable = true
        metalView.onTouch = { (event: TouchState, x: Int32, y: Int32) in
            let cutoff = Int32(self.PlatformState.RenderBuffer.Viewport.Height * 2 / 3)
            let landscape =
                self.PlatformState.RenderBuffer.Viewport.Height
                < self.PlatformState.RenderBuffer.Viewport.Width
            let right = x > self.PlatformState.RenderBuffer.Viewport.Width / 2
            let bottom = y > self.PlatformState.RenderBuffer.Viewport.Height / 2

            let draw = (right && landscape) || bottom && !landscape

            switch event {
            case .Begin:
                self.GameInput.Mouse.MouseX = x
                self.GameInput.Mouse.MouseY = y
                if !draw {
                    self.GameInput.Mouse.Buttons.0.EndedDown = true
                    self.GameInput.Mouse.Buttons.0.HalfTransitionCount += 1
                    self.GameInput.Mouse.InRange = true
                } else {
                    self.GameInput.Mouse.Buttons.1.EndedDown = true
                    self.GameInput.Mouse.Buttons.1.HalfTransitionCount += 1
                    self.GameInput.Mouse.InRange = false
                }
                break
            case .Move:
                self.GameInput.Mouse.DeltaX = x - self.GameInput.Mouse.MouseX
                self.GameInput.Mouse.DeltaY = y - self.GameInput.Mouse.MouseY
                self.GameInput.Mouse.MouseX = x
                self.GameInput.Mouse.MouseY = y
                break
            case .Ended:
                self.GameInput.Mouse.MouseX = x
                self.GameInput.Mouse.MouseY = y
                self.GameInput.Mouse.Buttons.0.EndedDown = false
                self.GameInput.Mouse.Buttons.1.EndedDown = false
                self.GameInput.Mouse.Buttons.2.EndedDown = false
                self.GameInput.Mouse.Buttons.0.HalfTransitionCount += 1

                self.GameInput.Mouse.InRange = false
                break
            }
        }

        metalView.delegate = self
        metalView.clearColor = MTLClearColor(red: 0, green: 1, blue: 1, alpha: 1)
        metalView.device = MTLCreateSystemDefaultDevice()
        metalView.enableSetNeedsDisplay = false
        metalView.isPaused = false
        let circleSize: CGFloat = metalView.bounds.width * 0.3

        let circleView = CircleOverlayView()

        circleView.onStick = { p in
            self.GameInput.Controllers.0.isAnalog = true
            if let (x,y) = p {
                self.GameInput.Controllers.0.AverageStickX = x
                self.GameInput.Controllers.0.AverageStickY = -y
            } else {
                self.GameInput.Controllers.0.AverageStickX = 0
                self.GameInput.Controllers.0.AverageStickY = 0
            }
        }
        circleView.translatesAutoresizingMaskIntoConstraints = false

        metalView.addSubview(circleView)

        NSLayoutConstraint.activate([
            circleView.widthAnchor.constraint(equalToConstant: circleSize),
            circleView.heightAnchor.constraint(equalToConstant: circleSize),

            circleView.trailingAnchor.constraint(
                equalTo: metalView.safeAreaLayoutGuide.trailingAnchor,
                constant: -16
            ),
            circleView.bottomAnchor.constraint(
                equalTo: metalView.safeAreaLayoutGuide.bottomAnchor,
                constant: -16
            )
        ])

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
        metalView.setNeedsLayout()
        metalView.layoutIfNeeded()  // force frame/layout update
        metalView.drawableSize = CGSize(
            width: metalView.bounds.width * metalView.contentScaleFactor,
            height: metalView.bounds.height * metalView.contentScaleFactor
        )

        return true
    }
    func draw(in view: MTKView) {

        var Context = thread_context()
        var CurrentInput = GameInput

        PlatformState.RenderBuffer.Count = 0

        PlatformState.RenderBuffer.Viewport.Inset.Style = RenderViewportInsetStyleVirtualTop;
        PlatformState.RenderBuffer.Viewport.Inset.Left = UInt32(metalView.safeAreaInsets.left * metalView.contentScaleFactor);
        PlatformState.RenderBuffer.Viewport.Inset.Right = UInt32(metalView.safeAreaInsets.right * metalView.contentScaleFactor);
        PlatformState.RenderBuffer.Viewport.Inset.Top = UInt32(metalView.safeAreaInsets.top * metalView.contentScaleFactor);
        PlatformState.RenderBuffer.Viewport.Inset.Bottom = UInt32(metalView.safeAreaInsets.bottom * metalView.contentScaleFactor);


        CurrentInput.DeltaTime = 0.01;
        GameUpdateAndRender(&Context, &GameMemory, &CurrentInput, &PlatformState.RenderBuffer)
        withUnsafeTemporaryAllocation(of: Int16.self, capacity: 100) { buffer in
            if let b = buffer.baseAddress {
                var SoundBuffer = game_sound_output_buffer()
                SoundBuffer.SamplesPerSecond = 44100
                SoundBuffer.SampleCount = 100
                SoundBuffer.Samples = b
                GameGetSoundSamples(&Context, &GameMemory, &SoundBuffer)
            }
        }

        self.GameInput.Mouse.Buttons.0.HalfTransitionCount = 0
        self.GameInput.Mouse.Buttons.1.HalfTransitionCount = 0
        self.GameInput.Mouse.Buttons.2.HalfTransitionCount = 0
        self.GameInput.Mouse.Buttons.3.HalfTransitionCount = 0
        self.GameInput.Mouse.Buttons.4.HalfTransitionCount = 0
        self.GameInput.Mouse.DeltaX = 0
        self.GameInput.Mouse.DeltaY = 0

        guard
            let drawable = view.currentDrawable,
            let passDescriptor = view.currentRenderPassDescriptor,
            let pipelineState = pipelineState,
            let device = view.device
        else { return }

        passDescriptor.colorAttachments[0].clearColor =
            MTLClearColor(red: 0, green: 1, blue: 0.5, alpha: 1)
        passDescriptor.colorAttachments[0].loadAction = .clear
        passDescriptor.colorAttachments[0].storeAction = .store

        let commandBuffer = view.device!.makeCommandQueue()!.makeCommandBuffer()!
        let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: passDescriptor)!

        encoder.setRenderPipelineState(pipelineState)

        var vertices: [MetalVertex] = []

        for i in 0..<PlatformState.RenderBuffer.Count {
            let cmd = PlatformState.RenderBuffer.Base[i]
            switch cmd.Type {
            case RenderCommandTriangle:
                break
            case RenderCommandRect:
                let rect = cmd.Rect
                let col = (rect.Color.Red, rect.Color.Green, rect.Color.Blue, rect.Color.Alpha)
                vertices.append(
                    MetalVertex(pos: (rect.MinX, rect.MinY), col: col, tex: (0.0, 0.0, 0.0)))
                vertices.append(
                    MetalVertex(pos: (rect.MinX, rect.MaxY), col: col, tex: (0.0, 0.0, 0.0)))
                vertices.append(
                    MetalVertex(pos: (rect.MaxX, rect.MaxY), col: col, tex: (0.0, 0.0, 0.0)))
                vertices.append(
                    MetalVertex(pos: (rect.MinX, rect.MinY), col: col, tex: (0.0, 0.0, 0.0)))
                vertices.append(
                    MetalVertex(pos: (rect.MaxX, rect.MaxY), col: col, tex: (0.0, 0.0, 0.0)))
                vertices.append(
                    MetalVertex(pos: (rect.MaxX, rect.MinY), col: col, tex: (0.0, 0.0, 0.0)))
            default: break
            }

        }

        let dataSize = MemoryLayout<MetalVertex>.stride * vertices.count
        let vertexBuffer = device.makeBuffer(bytes: vertices, length: dataSize, options: [])!

        encoder.setVertexBuffer(vertexBuffer, offset: 0, index: 0)
        var uni = MetalUniforms(
            scaleX: 2.0 / Float(PlatformState.RenderBuffer.Viewport.Width),
            scaleY: -2.0 / Float(PlatformState.RenderBuffer.Viewport.Height), transX: -1.0,
            transY: 1.0)

        encoder.setVertexBytes(
            &uni, length: MemoryLayout<MetalUniforms>.stride, index: 1)

        encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: vertices.count)
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
