export namespace Platform {
  export type Os = "darwin" | "linux";
  export type Arch = "arm64" | "x64";
  export type Libc = "apple" | "gnu";

  export type Target = {
    os: Os;
    arch: Arch;
    libc: Libc;
  };

  export const supported: Target[] = [
    { os: "darwin", arch: "arm64", libc: "apple" },
    { os: "darwin", arch: "x64", libc: "apple" },
    { os: "linux", arch: "arm64", libc: "gnu" },
    { os: "linux", arch: "x64", libc: "gnu" },
  ];

  export const triple = (target: Target) => `${target.arch}-${target.os}-${target.libc}`;
  export const packageName = (target: Target) => `@stevelock/stevelock-${triple(target)}`;
  export const packageDir = (target: Target) => `packages/stevelock-${triple(target)}`;

  export const detect = (): Target => {
    if (process.platform !== "linux" && process.platform !== "darwin") {
      throw new Error(`unsupported os: ${process.platform}`);
    }

    if (process.arch !== "x64" && process.arch !== "arm64") {
      throw new Error(`unsupported arch: ${process.arch}`);
    }

    const os: Os = process.platform;
    const arch: Arch = process.arch;
    const libc: Libc = os === "darwin" ? "apple" : "gnu";
    return { os, arch, libc };
  };
}
