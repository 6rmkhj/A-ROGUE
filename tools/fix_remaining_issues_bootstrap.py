from pathlib import Path

p = Path(__file__).with_name('fix_remaining_issues.py')
s = p.read_text(encoding='utf-8')
old = "        POINT p = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));"
new = "        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));"
count = s.count(old)
if count != 2:
    raise RuntimeError(f'expected 2 WM_MOUSEMOVE matcher lines, got {count}')
p.write_text(s.replace(old, new), encoding='utf-8', newline='\n')
print('remaining issue patch matcher updated')
