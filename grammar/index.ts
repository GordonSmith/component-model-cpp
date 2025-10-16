import type { LanguageInput } from "shiki";

import wit_lang from "../ref/vscode-wit/syntaxes/wit.tmLanguage.json" with { type: "json" };

export function witLang(): LanguageInput {
    const retVal = {
        ...wit_lang,
        name: "wit"
    } as unknown as LanguageInput;
    return retVal;
}

