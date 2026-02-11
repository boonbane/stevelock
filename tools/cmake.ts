import path from "path";

export namespace CMake {
  type Config = {
    cwd: string;
    source: string;
    buildDir: string;
    generator: string;
    buildType: string;
    defines: string[];
  };

  export type Builder = {
    source: (value: string) => Builder;
    buildDir: (value: string) => Builder;
    generator: (value: string) => Builder;
    buildType: (value: string) => Builder;
    define: (key: string, value: string) => Builder;
    defineIf: (key: string, value: string, pred: () => boolean) => Builder;
    configure: () => Builder;
    build: (target?: string) => Builder;
    install: (prefix?: string) => Builder;
    test: (name?: string) => Builder;
    run: (name: string) => Builder;
  };

  const run = (config: Config, args: string[]) => {
    const result = Bun.spawnSync(["cmake", ...args], {
      cwd: config.cwd,
      stdio: ["inherit", "inherit", "inherit"],
    });
    if (!result.success) {
      throw new Error(`cmake exited with code ${result.exitCode}`);
    }
  };

  export const cmake = (): Builder => {
    const config: Config = {
      cwd: process.cwd(),
      source: process.cwd(),
      buildDir: path.join(process.cwd(), ".cache", "build"),
      generator: "",
      buildType: "Release",
      defines: [],
    };

    const chain: Builder = {
      source(value) {
        config.cwd = value;
        config.source = value;
        return chain;
      },
      buildDir(value) {
        config.buildDir = value;
        return chain;
      },
      generator(value) {
        config.generator = value;
        return chain;
      },
      buildType(value) {
        config.buildType = value;
        return chain;
      },
      define(key, value) {
        config.defines.push(`-D${key}=${value}`);
        return chain;
      },
      defineIf(key, value, pred) {
        if (pred()) {
          config.defines.push(`-D${key}=${value}`);
        }
        return chain;
      },
      configure() {
        run(config, [
          "-S",
          config.source,
          "-B",
          config.buildDir,
          ...(config.generator ? ["-G", config.generator] : []),
          `-DCMAKE_BUILD_TYPE=${config.buildType}`,
          ...config.defines,
        ]);
        return chain;
      },
      build(target) {
        run(config, [
          "--build",
          config.buildDir,
          ...(target ? ["--target", target] : []),
          "--config",
          config.buildType,
        ]);
        return chain;
      },
      install(prefix) {
        run(config, [
          "--install",
          config.buildDir,
          ...(prefix ? ["--prefix", prefix] : []),
          "--config",
          config.buildType,
        ]);
        return chain;
      },
      test(name) {
        const result = Bun.spawnSync(
          [
            "ctest",
            "--test-dir",
            config.buildDir,
            "--output-on-failure",
            ...(name ? ["-R", name] : []),
          ],
          {
            cwd: config.cwd,
            stdio: ["inherit", "inherit", "inherit"],
          },
        );
        if (!result.success) {
          throw new Error(`ctest exited with code ${result.exitCode}`);
        }
        return chain;
      },
      run(name) {
        const result = Bun.spawnSync([path.join(config.buildDir, name)], {
          cwd: config.cwd,
          stdio: ["inherit", "inherit", "inherit"],
        });
        if (!result.success) {
          throw new Error(`${name} exited with code ${result.exitCode}`);
        }
        return chain;
      },
    };

    return chain;
  };

}
