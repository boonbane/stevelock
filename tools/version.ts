#!/usr/bin/env bun
import path from "path";

namespace Version {
  type Json = Record<string, unknown>;

  const root = path.join(import.meta.dir, "..");
  const rootManifest = path.join(root, "package.json");
  const manifests = [
    rootManifest,
    path.join(root, "packages", "stevelock-arm64-darwin-apple", "package.json"),
    path.join(root, "packages", "stevelock-x64-darwin-apple", "package.json"),
    path.join(root, "packages", "stevelock-arm64-linux-gnu", "package.json"),
    path.join(root, "packages", "stevelock-x64-linux-gnu", "package.json"),
    path.join(root, "example", "local", "package.json"),
    path.join(root, "example", "remote", "package.json"),
  ] as const;
  type Version = {
    major: number;
    minor: number;
    patch: number;
  };

  const isObject = (value: unknown): value is Json => {
    if (value === null) {
      return false;
    }
    if (Array.isArray(value)) {
      return false;
    }
    return typeof value === "object";
  };

  const bumpRootOptional = (manifest: Json, version: string) => {
    const optional = manifest.optionalDependencies;
    if (!isObject(optional)) {
      return;
    }

    const next = Object.fromEntries(
      Object.entries(optional).map(([name, value]) => {
        if (!name.startsWith("@stevelock/stevelock-")) {
          return [name, value];
        }
        return [name, version];
      }),
    );

    manifest.optionalDependencies = next;
  };

  const bumpManifest = async (file: string, version: string): Promise<boolean> => {
    const manifest = (await Bun.file(file).json()) as unknown;
    if (!isObject(manifest)) {
      return false;
    }
    if (typeof manifest.version !== "string") {
      return false;
    }

    const next = { ...manifest };
    next.version = version;

    if (file === rootManifest) {
      bumpRootOptional(next, version);
    }

    await Bun.write(file, `${JSON.stringify(next, null, 2)}\n`);
    return true;
  };

  const parseCore = (value: string): string => {
    const dash = value.indexOf("-");
    const plus = value.indexOf("+");
    const cut = [dash, plus].filter((it) => it >= 0).sort((a, b) => a - b)[0];
    if (cut === undefined) {
      return value;
    }
    return value.slice(0, cut);
  };

  const parseVersion = (value: string): Version | null => {
    const core = parseCore(value.trim());
    const parts = core.split(".");
    if (parts.length !== 3) {
      return null;
    }

    const major = Number(parts[0]);
    const minor = Number(parts[1]);
    const patch = Number(parts[2]);
    if (!Number.isInteger(major) || major < 0) {
      return null;
    }
    if (!Number.isInteger(minor) || minor < 0) {
      return null;
    }
    if (!Number.isInteger(patch) || patch < 0) {
      return null;
    }

    return { major, minor, patch };
  };

  const resolveVersion = async (input: string): Promise<string> => {
    if (parseVersion(input)) {
      return input;
    }

    if (input !== "major" && input !== "minor" && input !== "patch") {
      throw new Error(`invalid version bump: ${input}`);
    }

    const manifest = (await Bun.file(rootManifest).json()) as unknown;
    if (!isObject(manifest)) {
      throw new Error(`invalid root manifest: ${rootManifest}`);
    }

    const current = manifest.version;
    if (typeof current !== "string") {
      throw new Error(`missing version in ${rootManifest}`);
    }

    const parsed = parseVersion(current);
    if (!parsed) {
      throw new Error(`cannot parse current version: ${current}`);
    }

    if (input === "major") {
      return `${parsed.major + 1}.0.0`;
    }
    if (input === "minor") {
      return `${parsed.major}.${parsed.minor + 1}.0`;
    }
    return `${parsed.major}.${parsed.minor}.${parsed.patch + 1}`;
  };

  export const main = async () => {
    const arg = String(process.argv[2] ?? "").trim();
    if (!arg) {
      throw new Error("usage: bun run tools/version.ts <version|patch|minor|major>");
    }

    const version = await resolveVersion(arg);

    const touched = [] as string[];
    for (const file of manifests) {
      if (!await bumpManifest(file, version)) {
        continue;
      }
      touched.push(path.relative(root, file));
    }

    if (touched.length === 0) {
      throw new Error("no package.json files with a version field");
    }

    for (const file of touched) {
      console.log(`updated ${file} -> ${version}`);
    }
  };
}

await Version.main();
