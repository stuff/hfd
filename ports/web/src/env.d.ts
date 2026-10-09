declare module '*?indexed' {
  const img: { width: number; height: number; data: Uint8Array };
  export default img;
}

// AudioWorklet globals (not in lib.dom).
declare const sampleRate: number;
declare const currentFrame: number;
declare class AudioWorkletProcessor {
  readonly port: MessagePort;
  constructor();
}
declare function registerProcessor(
  name: string,
  ctor: new () => AudioWorkletProcessor & {
    process(inputs: Float32Array[][], outputs: Float32Array[][], params: Record<string, Float32Array>): boolean;
  },
): void;
