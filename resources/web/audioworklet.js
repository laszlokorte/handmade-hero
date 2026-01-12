class WasmProcessor extends AudioWorkletProcessor {
  #memory;
  #instance;
  constructor(...args) {
    const [
      {
        processorOptions: { memory, wasm },
      },
    ] = args;
    super(...args);
    this.memory = memory;
    WebAssembly.instantiate(wasm, {
      env: { memory },
    }).then((i) => {
      this.instance = i;
    });
  }
  static get parameterDescriptors() {
    return [];
  }
  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const blockSize = output[0].length;
    const ptr = this.instance.exports.output_audio(blockSize);

    const samples = new Float32Array(this.memory.buffer, ptr, blockSize * 2);

    const channelCount = output.length;
    for (let c = 0; c < channelCount; c++) {
      const channel = output[c];
      for (let i = 0; i < blockSize; i++) {
        channel[i] = samples[2 * i + c];
      }
    }
    return true;
  }
}

registerProcessor("wasm-processor", WasmProcessor);
