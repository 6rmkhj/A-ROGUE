from pathlib import Path

p = Path(__file__).with_name('fix_remaining_issues.py')
s = p.read_text(encoding='utf-8')

# Current main stores mouse position directly in gMouse rather than a POINT p.
old = "        POINT p = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));"
new = "        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));"
count = s.count(old)
if count == 2:
    s = s.replace(old, new)
elif count != 0:
    raise RuntimeError(f'unexpected WM_MOUSEMOVE matcher count: {count}')

# re.sub replacement strings interpret backslash escapes. The patch payload contains
# C++ \\n sequences and one capture reference, so expand the capture ourselves and otherwise
# return the replacement verbatim.
old_rx = "    out,n=re.subn(pat,repl,s,count=1,flags=re.S)"
new_rx = "    out,n=re.subn(pat,lambda m: repl.replace('\\\\1', m.group(1) if m.lastindex else ''),s,count=1,flags=re.S)"
if old_rx in s:
    s = s.replace(old_rx, new_rx, 1)
elif new_rx not in s:
    raise RuntimeError('rx helper shape changed')

# Make the BeginNewRun replacement keep the capture token as two source characters
# instead of Python turning \1 into U+0001 before rx() sees it.
old_begin = "'''static void BeginNewRun() {\\1\n"
new_begin = "r'''static void BeginNewRun() {\\1\n"
if old_begin in s:
    s = s.replace(old_begin, new_begin, 1)
elif new_begin not in s:
    raise RuntimeError('BeginNewRun replacement shape changed')

p.write_text(s, encoding='utf-8', newline='\n')
print('remaining issue patch generator updated')
