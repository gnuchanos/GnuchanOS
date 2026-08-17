import * as monaco from "monaco-editor";
import { loader } from "@monaco-editor/react";
import editorWorker from "monaco-editor/esm/vs/editor/editor.worker?worker";

self.MonacoEnvironment = {
  getWorker: () => new editorWorker(),
};

loader.config({ monaco });

/* ---- GCL dil tanimi (.gcsf / .gclib / .gcl) ----
 * Daha once yalnizca "plaintext" olarak dusen GCL dosyalari artik
 * Monaco tokenizer ile syntax highlight alir: keyword / string /
 * number / comment / type / preprocessor. */

const GCL_KEYWORDS = [
  "int", "float", "double", "char", "bool", "void", "long", "short",
  "unsigned", "signed", "const", "static", "struct", "typedef", "enum",
  "union", "return", "if", "else", "while", "for", "do", "switch",
  "case", "break", "continue", "default", "sizeof", "goto", "define",
  "include", "main",
];

const GCL_BUILTINS = [
  "printf", "sprintf", "snprintf", "scanf", "sscanf", "fprintf",
  "malloc", "calloc", "realloc", "free", "memcpy", "memset", "strlen",
  "strcpy", "strcmp", "strcat", "exit", "abs", "fopen", "fclose",
  "fread", "fwrite", "fgets", "fputs", "assert",
];

monaco.languages.register({ id: "gcl" });

monaco.languages.setMonarchTokensProvider("gcl", {
  keywords: GCL_KEYWORDS,
  builtins: GCL_BUILTINS,
  tokenizer: {
    root: [
      /* preprocessor: #define / #include */
      [/^#\s*[A-Za-z_][\w]*/, "preprocessor"],
      [/^#\s*include\s+[<"][^>"]+[>"]/, "preprocessor.string"],

      /* comments */
      [/\/\*/, "comment", "@comment"],
      [/\/\/.*$/, "comment"],

      /* strings + char literals */
      [/"(\\.|[^"\\])*"/, "string"],
      [/'(?:\\.|[^'\\])'/, "string"],

      /* numbers */
      [/\b0[xX][0-9a-fA-F]+\b/, "number"],
      [/\b\d+\.\d+([eE][+-]?\d+)?\b/, "number.float"],
      [/\b\d+([eE][+-]?\d+)?\b/, "number"],

      /* identifiers */
      [
        /[A-Za-z_]\w*/,
        {
          cases: {
            "@keywords": "keyword",
            "@builtins": "type.identifier",
            "@default": "identifier",
          },
        },
      ],

      /* operators + punctuation */
      [/[{}()\[\]]/, "@brackets"],
      [/[;:,.]/, "delimiter"],
      [/[+\-*/%<>=!&|^~]=?/, "operator"],
    ],
    comment: [
      [/[^/*]+/, "comment"],
      [/\*\//, "comment", "@pop"],
      [/[/*]/, "comment"],
    ],
  },
});

monaco.languages.setLanguageConfiguration("gcl", {
  comments: { lineComment: "//", blockComment: ["/*", "*/"] },
  brackets: [
    ["{", "}"],
    ["[", "]"],
    ["(", ")"],
  ],
  autoClosingPairs: [
    { open: "{", close: "}" },
    { open: "[", close: "]" },
    { open: "(", close: ")" },
    { open: '"', close: '"' },
    { open: "'", close: "'" },
  ],
  surroundingPairs: [
    { open: "{", close: "}" },
    { open: "[", close: "]" },
    { open: "(", close: ")" },
    { open: '"', close: '"' },
  ],
});

export { monaco };

