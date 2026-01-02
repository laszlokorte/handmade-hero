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
    const blockSize = outputs[0][0].length;
    const ptr = this.instance.exports.output_audio(blockSize);

    const samples = new Float32Array(this.memory.buffer, ptr, blockSize);

    const output = outputs[0];
    output.forEach((channel) => {
      for (let i = 0; i < blockSize; i++) {
        channel[i] = samples[2 * i];
      }
    });
    return true;
  }
}

registerProcessor("wasm-processor", WasmProcessor);
