import fs from "fs";

const main = () => {
  const jail = process.argv[2] ?? process.cwd();
  const escape = process.argv[3] ?? `${jail}/../stevelock-escape.txt`;
  const out = `${jail}/output.txt`;
  fs.writeFileSync(out, "hello from jail");
  fs.writeFileSync(escape, "escaped");
  console.log("escape write: allowed");
  return 0;
};

process.exit(main());
