# 桌面黑块故障

## 实际故障

用户在显示桌面后提供截图：便签位置出现完整黑色矩形，标题、任务和解锁按钮
不可见。因此此前窗口样式、父子关系、集成测试不能证明桌面视觉验收通过。
检查默认数据确认，新建的默认便签未锁定，该问题不限于鼠标穿透状态。

## 本次改动

- 桌面模式正文及独立解锁按钮使用分层窗口的显式像素提交。
- 将 GDI 绘制内容复制到 32 位 DIB，补全不透明 alpha，通过
  `UpdateLayeredWindow` 提交，不依赖 Explorer 对跨进程子窗口的普通 GDI 重定向。
- 删除 `SetLayeredWindowAttributes` 路径，避免与显式像素提交相冲突。
- 桌面未锁定时仍显式提交图像，但不设置输入穿透；原生编辑框内容纳入图像，
  编辑通知使便签重绘。
- 绘制提交失败会写入应用错误状态，不把 API 调用失败当作成功。

Microsoft 文档：

- [分层窗口绘制](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
- [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)

## 验证与边界

37 项原生无头测试通过，其中新增隐藏子窗口的 GDI 颜色、重复像素提交、锁定到
未锁定的重新提交测试。49 项可执行程序集成检查通过，新增真实 Explorer 桌面
正文与解锁按钮强制绘制，以及重复锁定/解锁绘制检查。

已重建并重新打开测试便签，强制绘制命令未产生错误。
用户随后反馈桌面不再显示便签，证明仅调整像素提交仍未完成修复。

进一步读取当前 Explorer 的实际窗口层次，发现：

- Progman 扩展样式为 `0x200080`，包含 `WS_EX_NOREDIRECTIONBITMAP`。
- 其 `SHELLDLL_DefView` 子窗口扩展样式为 `0x80000`，包含 `WS_EX_LAYERED`；
  该窗口内有实际显示桌面图标的 `SysListView32`。
- 便签已经成功提交过像素，故不能简单归因于没有收到首次绘制消息。

已为这类宿主选择实际桌面内容窗口作为父窗口，不修改任何 Explorer 样式。
普通宿主仍保持原策略。新增原生测试覆盖带无重定向样式的宿主、普通宿主和
无效父子关系。必须在创建窗口时设置该样式，创建后设置会被系统忽略，测试不
假设一次 `SetWindowLongPtrW` 就必然改变此样式。

重新运行后，真实便签挂接到 `SHELLDLL_DefView`，像素提交成功。
尚需用户确认“显示桌面”后的实际图像和交互；不能仅凭 API 成功断言故障消失。
