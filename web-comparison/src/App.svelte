<script lang="ts">
  import { onMount, tick } from 'svelte';
  import Button from 'flowbite-svelte/Button.svelte';
  import Input from 'flowbite-svelte/Input.svelte';
  import Select from 'flowbite-svelte/Select.svelte';
  import Checkbox from 'flowbite-svelte/Checkbox.svelte';
  import Lock from '@lucide/svelte/icons/lock';
  import LockOpen from '@lucide/svelte/icons/lock-open';
  import Plus from '@lucide/svelte/icons/plus';
  import GripVertical from '@lucide/svelte/icons/grip-vertical';
  import StickyNote from '@lucide/svelte/icons/sticky-note';
  import { dndzone } from 'svelte-dnd-action';
  import { request, visibleTasks, hex, type Snapshot, type Task } from './protocol';
  let state = $state<Snapshot>();
  let items = $state<Task[]>([]);
  let text = $state('');
  let error = $state('');
  const filters = ['全部', '未完成', '待办', '进行中', '已完成'].map((name, value) => ({ name, value }));
  const modes = ['桌面', '置顶', '窗口'].map((name, value) => ({ name, value }));
  async function receive(next: Snapshot) {
    state = next;
    items = visibleTasks(next.note);
    error = next.error ?? '';
    await tick();
    requestAnimationFrame(() => requestAnimationFrame(() => {
      window.comparisonReady = true;
      void request('ready', next.revision);
    }));
  }
  async function act(action: string, ...args: unknown[]) {
    try { await receive(await request(action, ...args)); }
    catch (e) { error = String(e); }
  }
  onMount(() => { window.receiveNote = receive; void act('snapshot'); });
  async function add(event: SubmitEvent) {
    event.preventDefault();
    if (!text.trim()) return;
    await act('add', text);
    if (!error) text = '';
  }
</script>

{#if state}
  <main style:--note-color={hex(state.note.color)}>
    <header>
      <StickyNote size={20} />
      <h1>{state.note.title}</h1>
      <Button size="xs" color="alternative" class="lock-button" aria-label={state.note.locked ? '解锁' : '锁定'} onclick={() => act('lock', !state!.note.locked)}>
        {#if state.note.locked}<Lock size={18} />{:else}<LockOpen size={18} />{/if}
      </Button>
    </header>
    <div class="toolbar">
      <Select aria-label="筛选待办" items={filters} value={state.note.filter} onchange={(event) => act('filter', Number(event.currentTarget.value))} size="sm" />
      <Select aria-label="窗口模式" items={modes} value={state.note.mode} onchange={(event) => act('mode', Number(event.currentTarget.value))} size="sm" />
    </div>
    <section aria-label="待办列表" use:dndzone={{ items, flipDurationMs: 0, dragDisabled: state.note.locked }}
      onconsider={(event) => items = event.detail.items}
      onfinalize={(event) => { items = event.detail.items; void act('order', items.map(item => item.id)); }}>
      {#each items as task (task.id)}
        <div class="task" style:color={hex(task.color)}>
          <span class="grip" aria-hidden="true"><GripVertical size={15} /></span>
          <Checkbox aria-label={`完成 ${task.text}`} checked={task.status === 2} disabled={state.note.locked} onchange={() => act('status', task.id, task.status === 2 ? 0 : 2)} />
          <span class="task-icon" aria-hidden="true">{task.icon}</span>
          <span class:done={task.status === 2} class="task-text">{task.text}</span>
          <button class="status" disabled={state.note.locked} aria-label={`切换状态 ${task.text}`} onclick={() => act('status', task.id, (task.status + 1) % 3)}>{['待办', '进行中', '完成'][task.status]}</button>
        </div>
      {/each}
    </section>
    <form onsubmit={add}>
      <Input aria-label="新待办" placeholder="添加一项待办…" bind:value={text} disabled={state.note.locked} />
      <Button type="submit" size="sm" aria-label="添加待办" disabled={state.note.locked}><Plus size={19} /></Button>
    </form>
    <footer>{state.note.tasks.length} 项 · Svelte 对比实验</footer>
    {#if error}<p role="alert">{error}</p>{/if}
  </main>
{:else}
  <p role="status">{error || '正在加载便签…'}</p>
{/if}
