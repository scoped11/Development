# arsenal_run

MCP: `driverlab-crack` → `arsenal_run`

| Workflow | Args |
|----------|------|
| `profile` | `--preset siv` or `--sys <path>` |
| `qualify` | `--preset siv\|sonix\|ioc` |
| `extract_profile` | `--installer <path>` |

```powershell
python driver-lab\arsenal\arsenal_run.py qualify --preset siv
```

Qualify opens an existing device before it stages a second service. Lab only; no anti-cheat bypass.
