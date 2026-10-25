"""Render measured results without hiding process or startup accounting differences."""
import hashlib
import json
from pathlib import Path
import platform
import statistics
import psutil

root = Path(__file__).resolve().parent.parent
path = root / 'docs/web-comparison-results.json'
result = json.loads(path.read_text(encoding='utf8'))
base = root / '.tools/web-comparison'
runtime = Path('C:/Program Files (x86)/Microsoft/EdgeWebView/Application')
versions = [p.name for p in runtime.iterdir() if p.is_dir() and p.name[0].isdigit()]
result['environment'] = {'platform': platform.platform(), 'logicalProcessors': psutil.cpu_count(),
                         'installedWebView2Versions': versions, 'psutil': psutil.__version__}
result['webHostSha256'] = hashlib.sha256((base / 'bin/DesktopTodo.WebComparison.exe').read_bytes()).hexdigest()
result['artifactBytes'] = {
    'nativeExe': (root / 'dist/DesktopTodo.exe').stat().st_size,
    'webExe': (base / 'bin/DesktopTodo.WebComparison.exe').stat().st_size,
    'webLoader': (base / 'bin/WebView2Loader.dll').stat().st_size,
    'frontend': sum(p.stat().st_size for p in (base / 'frontend').rglob('*') if p.is_file())}
path.write_text(json.dumps(result) + '\n', encoding='utf8')


def median(samples, key):
    return statistics.median(sample[key] for sample in samples)


lines = ['# 原生版与 Svelte / 系统 WebView2 对比', '',
         f"测量时间：{result['measuredAt']}。Windows 11，{psutil.cpu_count()} 个逻辑处理器，WebView2 {', '.join(versions)}。", '',
         '保留现有 C++/Win32 版本。实验采用 Svelte 5、Flowbite Svelte、Lucide、svelte-dnd-action，',
         '通过 webview/webview 封装使用已安装的微软 WebView2 Runtime；没有打包 Chromium。',
         '待办规则、Controller 和 SQLite 直接复用现有 C++ 实现。每张便签一个 WebView，同一宿主共享浏览器进程组。', '',
         '## 普通窗口结果', '',
         '下表为每档 3 次新建数据库/浏览器缓存运行的中位数。窗口解锁，全部重叠，外框均为 680×760 物理像素。',
         '启动后静置 3 秒，再测 10 秒空闲 CPU；CPU 百分比以一个核心为 100%。', '',
         '| 负载 | 实现 | 进程数 | 私有提交 MiB | 工作集合计 MiB | 空闲 CPU（单核） | 状态命令往返 ms |',
         '|---|---|---:|---:|---:|---:|---:|']
for notes in [1, 10]:
    for kind, label in [('native', 'C++ / Win32'), ('web', 'Svelte / WebView2')]:
        samples = [s for s in result['samples'] if s['notes'] == notes and s['kind'] == kind and s['phase'] == 'fresh-profile']
        values = [median(samples, k) for k in ['processCount', 'privateBytesSumMiB', 'workingSetSumMiB', 'oneCoreCpuPercent', 'statusRoundTripMedianMs']]
        lines.append(f"| {notes} 便签 × {samples[0]['tasksPerNote']} 项 | {label} | {values[0]:g} | {values[1]:.2f} | {values[2]:.2f} | {values[3]:.3f}% | {values[4]:.2f} |")
lines += ['', '私有提交是进程承诺使用的私有内存，不是物理驻留量；工作集合计会重复计算共享页面。',
          '两者均统计完整进程树，包括 WebView2 的浏览器、渲染器、GPU 和工具进程；不是只看小型宿主 exe。',
          '状态命令时间包含 IPC、持久化和快照文件读写，不包含 UI 渲染完成。',
          '遮挡窗口可能暂停动画帧，因此不把它当作后台交互完成条件；这个实验不能据此判定某版点击更快。', '',
          '## 缓存复用与启动准备', '',
          '每档最后一轮复用同一目录再次启动一次。这里是缓存复用样本，不是多次中位数。', '',
          '| 负载 | 实现 | 准备完成秒 | 私有提交 MiB | 空闲 CPU（单核） |',
          '|---|---|---:|---:|---:|']
for sample in result['samples']:
    if sample['phase'] == 'reused-profile':
        label = 'C++ / Win32' if sample['kind'] == 'native' else 'Svelte / WebView2'
        lines.append(f"| {sample['notes']} 便签 × {sample['tasksPerNote']} 项 | {label} | {sample['launchToPreparedWorkloadSeconds']:.3f} | {sample['privateBytesSumMiB']:.2f} | {sample['oneCoreCpuPercent']:.3f}% |")
lines += ['', '原生版先启动，再通过自动化接口建样本和切换每个窗口；Web 宿主先建样本再创建视图。',
          '因此“准备完成”包含不同的造数流程，不能当成相同已有数据库的启动速度比；原始 JSON 还保留初始快照时间。',
          '新建浏览器缓存不代表操作系统冷缓存；没有控制系统文件缓存或其他日常桌面进程。', '',
          '## 产物与限制', '']
for key, value in result['artifactBytes'].items():
    lines.append(f'- {key}: {value:,} bytes')
lines += ['', 'WebView2 Runtime 为系统已安装的共享依赖，上述产物大小不包含运行时安装包和浏览器缓存。',
          '资源结果只适用于当前每便签独立 WebView 的实现。单个 WebView 承载多卡片的架构没有测试。',
          'Web 实验还没有接入桌面嵌入、Explorer 重启恢复和系统鼠标穿透，也未接入完整设置、备份和自启动界面。',
          '因此它不是生产版本的完整替代品；桌面模式显式报错。锁定仅禁用编辑控件，不表示通过了桌面穿透验收。', '',
          '## 验证', '',
          '- `npm run check`：行数门禁、门禁测试与 3 项前端无头测试。',
          '- `npm run test:native`：51 项原生核心测试及历史 SDK 实验测试。',
          '- `python scripts/test-web-comparison.py`：13 项真实编译宿主集成检查。',
          '- 原生产品回归：55 项应用集成与 20 项存储集成检查通过。', '',
          '[运行与复现步骤](../web-comparison/README.md) · [完整测量数据](web-comparison-results.json) · [宿主集成结果](web-comparison-contract.json)', '']
(root / 'docs/web-comparison.md').write_text('\n'.join(lines), encoding='utf8')
print('\n'.join(lines))
