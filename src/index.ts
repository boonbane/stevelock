import { createRequire } from "module";
import { Readable } from "stream";
import { constants } from "os";
import fs from "fs";
import path from "path";

const require = createRequire(import.meta.url);

const plat = process.platform;
const arch = process.arch;

const pkg =
  plat === "darwin" && arch === "arm64"
    ? "@stevelock/stevelock-darwin-arm64-apple"
    : plat === "darwin" && arch === "x64"
      ? "@stevelock/stevelock-darwin-x64-apple"
      : plat === "linux" && arch === "arm64"
        ? "@stevelock/stevelock-linux-arm64-gnu"
        : plat === "linux" && arch === "x64"
          ? "@stevelock/stevelock-linux-x64-gnu"
          : "";

const triple =
  plat === "darwin" && arch === "arm64"
    ? "darwin-arm64-apple"
    : plat === "darwin" && arch === "x64"
      ? "darwin-x64-apple"
      : plat === "linux" && arch === "arm64"
        ? "linux-arm64-gnu"
        : plat === "linux" && arch === "x64"
          ? "linux-x64-gnu"
          : "";

const local = triple
  ? path.join(import.meta.dir, "..", ".cache", "build", "native", triple, "Release", "stevelock.node")
  : "";
const prebuiltRoot = pkg ? path.join(import.meta.dir, "..", "node_modules", pkg, "stevelock.node") : "";
const prebuiltHost = pkg ? path.join(import.meta.dir, "..", "..", pkg, "stevelock.node") : "";
const prebuilt =
  prebuiltRoot && fs.existsSync(prebuiltRoot)
    ? prebuiltRoot
    : prebuiltHost && fs.existsSync(prebuiltHost)
      ? prebuiltHost
      : "";

const native = prebuilt && fs.existsSync(prebuilt) ? require(prebuilt) : require(local);

export interface SandboxOpts {
  /** directories the sandboxed process can write to */
  writable: string[];
  /** allow network access (default: false) */
  allowNet?: boolean;
  /** additional readable paths */
  readable?: string[];
}

export interface Sandbox {
  /** spawn a process inside the sandbox */
  spawn(cmd: string, args?: string[]): void;
  /** child pid (-1 if not spawned) */
  pid(): number;
  /** fd you write to for child stdin */
  stdinFd(): number;
  /** fd you read from for child stdout */
  stdoutFd(): number;
  /** fd you read from for child stderr */
  stderrFd(): number;
  /** readable stream of child stdout */
  stdout(): Readable;
  /** readable stream of child stderr */
  stderr(): Readable;
  /** blocking wait for exit. returns exit code. */
  wait(): number;
  /** send a signal to the child */
  kill(signal?: number): void;
  /** kill if running, free all resources */
  destroy(): void;
}

export function create(opts: SandboxOpts): Sandbox {
  const handle = native.create({
    writable: opts.writable,
    allowNet: opts.allowNet ?? false,
    readable: opts.readable ?? [],
  });

  let destroyed = false;

  return {
    spawn(cmd: string, args: string[] = []) {
      native.spawn(handle, cmd, args);
    },

    pid(): number {
      return native.pid(handle);
    },

    stdinFd(): number {
      return native.stdinFd(handle);
    },

    stdoutFd(): number {
      return native.stdoutFd(handle);
    },

    stderrFd(): number {
      return native.stderrFd(handle);
    },

    stdout(): Readable {
      const fd = native.stdoutFd(handle);
      if (fd < 0) throw new Error("not spawned");
      return Readable.fromWeb(
        new ReadableStream({
          start(controller) {
            const file = Bun.file(fd);
            const reader = file.stream().getReader();
            (async () => {
              try {
                while (true) {
                  const { done, value } = await reader.read();
                  if (done) break;
                  controller.enqueue(value);
                }
              } catch {
                /* fd closed */
              } finally {
                controller.close();
              }
            })();
          },
        })
      );
    },

    stderr(): Readable {
      const fd = native.stderrFd(handle);
      if (fd < 0) throw new Error("not spawned");
      return Readable.fromWeb(
        new ReadableStream({
          start(controller) {
            const file = Bun.file(fd);
            const reader = file.stream().getReader();
            (async () => {
              try {
                while (true) {
                  const { done, value } = await reader.read();
                  if (done) break;
                  controller.enqueue(value);
                }
              } catch {
                /* fd closed */
              } finally {
                controller.close();
              }
            })();
          },
        })
      );
    },

    wait(): number {
      return native.wait(handle);
    },

    kill(signal: number = constants.signals.SIGTERM) {
      native.kill(handle, signal);
    },

    destroy() {
      if (destroyed) return;
      destroyed = true;
      native.destroy(handle);
    },
  };
}
