NAMES = ["Keyword","Decl","Package","Flow","PrimitiveType","Type","Function",
         "Number","String","Comment","Preprocessor","Global","Field","Text",
         "Operator","Literal","Property"]

# base = upstream default colors, overridden per language
BASE = {"Keyword":"808000","PrimitiveType":"808000","Type":"800080","Function":"00677c",
        "Number":"000080","String":"008000","Comment":"008000","Preprocessor":"000080",
        "Global":"ce5c00","Field":"800000","Text":"000000","Operator":"","Literal":"B00020",
        "Property":"1A56AA","Decl":"007A00","Package":"2A8A9A","Flow":"8A0099"}

THEMES = {
 "bash":  {"Keyword":"007A00","Function":"0033CC","String":"7A6200","Comment":"00807A","Global":"B00020","Preprocessor":"007A00","Literal":"B00020","PrimitiveType":"007A00"},
 "python":{"Keyword":"0090A0","Function":"0033CC","String":"1F7A1F","Comment":"B03A2E","PrimitiveType":"007A00","Preprocessor":"2A8A9A"},
 "c":     {"Keyword":"8A6D00","Decl":"007A00","PrimitiveType":"007A00","String":"7A6200","Comment":"2A5ACC","Preprocessor":"2A8A9A"},
 "cxx":   {"Keyword":"1F5FA8","PrimitiveType":"007A8A","String":"7A1F1F","Comment":"008000","Preprocessor":"2A5ACC","Literal":"8A0099"},
 "csharp":{"Keyword":"1A56AA","PrimitiveType":"2B91AF","String":"A31515","Comment":"008000","Preprocessor":"2A5ACC","Literal":"6A0099"},
 "java":  {"Keyword":"B00000","PrimitiveType":"007A00","String":"A02020","Comment":"2A5ACC","Literal":"8A6D00"},
 "js":    {"Keyword":"007A5A","PrimitiveType":"007A5A","String":"8A0099","Comment":"2A5ACC","Number":"007A8A","Literal":"007A8A"},
 "go":    {"Keyword":"8A6D00","Decl":"007A00","Package":"2A8A9A","Flow":"8A0099","Literal":"B00020","PrimitiveType":"007A00","String":"1F7A1F","Comment":"007A5A","Number":"B00020","Function":"0033CC"},
 "rust":  {"Keyword":"A04000","PrimitiveType":"2E6B8A","String":"1F7A1F","Comment":"6A737D","Literal":"7A0099","Preprocessor":"2A8A9A"},
 "lua":   {"Keyword":"1A56CC","PrimitiveType":"1A56CC","String":"1F7A1F","Comment":"008000","Function":"8A6D00","Preprocessor":"2A8A9A"},
 "glsl":  {"Keyword":"00695C","PrimitiveType":"007A8A","String":"7A1F1F","Comment":"008000","Preprocessor":"2A5ACC"},
 "json":  {"Keyword":"6A0099","String":"0A6A6A","Number":"007A00"},
 "xml":   {"Keyword":"007A8A","Text":"B00020","String":"8A0099","Comment":"808000"},
 "html":  {"Keyword":"007A8A","Text":"B00020","String":"1F7A1F","Comment":"808000"},
 "sql":   {"Keyword":"0000CC","Function":"00677c","String":"1F7A1F","Comment":"3A7A3A","Number":"B00020","Literal":"B00020"},
 "php":   {"Keyword":"1A56AA","PrimitiveType":"2B91AF","String":"7A1F1F","Comment":"008000","Global":"B00020","Literal":"6A0099"},
 "css":   {"Property":"1A56AA","String":"1F7A1F","Comment":"808000","Preprocessor":"2A5ACC","Global":"B00020","Number":"000080","Keyword":"808000"},
 "ini":   {"Keyword":"1A56AA","Field":"800000","String":"1F7A1F","Comment":"6A737D","Literal":"8A0099"},
}

for lang, over in THEMES.items():
    pal = dict(BASE); pal.update(over)
    lines = ['<?xml version="1.0" encoding="UTF-8" ?>',
             f'<!-- Per-language style for MyTetra code blocks ({lang}).',
             '     Same style names as default_style.xml; colors follow the',
             '     language\'s nano color scheme, darkened for light background. -->',
             f'<style-scheme version="1.0" name="Mytetra-{lang}">']
    for name in NAMES:
        if lang == "lua" and name == "Operator":
            continue  # upstream lua data holds regexes, keep it colorless
        lines.append(f'    <style name="{name}" foreground="#{pal[name]}"/>')
    lines.append('</style-scheme>')
    open(f'{lang}.xml', 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print(lang, 'ok')
