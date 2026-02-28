# nightly_debugs
This repository contains documentation of Labs that I created for breaking and debugging systems. Concepts include linux, networks, k8s and eBPF
# Documentation as Portfolio
Every rep I have done for 8 months should be documented in a consistent format. This is both a reference and a proof of work.

Documentation Template — use this for every scenario
Scenario: [one sentence — what was broken] Environment: [OS, kernel version, tools used] Symptoms: [what you observed first] Hypotheses: [what I thought it could be — list all] Debugging Steps: [numbered — command, output, what it told you] Root Cause: [one sentence] Fix: [exact commands] Lesson: [what instinct this built — what you will recognize faster next time]

• each scenario is a markdown file
• Folder structure: /linux, /networking, /kubernetes, /ebpf — mirroring this roadmap
• each entry is tagged with: tools used, kernel subsystem, difficulty, time to resolve
• By month 8 target is to have 60-80 documented scenarios.