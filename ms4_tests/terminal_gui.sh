#!/usr/bin/env bash

# ms4 part 4, terminal gui for the risc-v simulator
# plain bash and standard terminal commands only, no extra packages needed

set -u

# the gui lives in ms4_tests so the project root is one level up
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR" || exit 1

BP_MODE=2
INPUT_FILE=""
ENABLE_CACHE=1
ENABLE_FORWARDING=1
DUMP_REGISTERS=0
DISASSEMBLE=0
INITIALIZE_REGISTERS=0
RUN_UNTIL_ECALL=1

bold='\033[1m'
cyan='\033[36m'
green='\033[32m'
yellow='\033[33m'
red='\033[31m'
reset='\033[0m'

clear_screen() {
    printf '\033[2J\033[H'
}

line() {
    printf '%s\n' "============================================================"
}

pause_screen() {
    printf '\nPress Enter to continue...'
    read -r _
}

on_off() {
    if [[ "$1" -eq 1 ]]; then printf 'ON'; else printf 'OFF'; fi
}

predictor_name() {
    case "$BP_MODE" in
        0) printf 'Disabled' ;;
        1) printf '1-bit last outcome' ;;
        2) printf '2-bit saturating counter' ;;
        *) printf 'Unknown' ;;
    esac
}

find_default_input() {
    local candidate
    for candidate in \
        ms4_tests/input/branch_predictor_2bit.input \
        ms4_tests/input/branch_predictor_1bit.input \
        ms4_tests/input/icache_basic.input \
        code/input/FLUSH.input; do
        if [[ -f "$candidate" ]]; then
            INPUT_FILE="$candidate"
            return
        fi
    done

    candidate="$(find code -type f -name '*.input' 2>/dev/null | head -n 1 || true)"
    INPUT_FILE="$candidate"
}

show_header() {
    clear_screen
    printf "${bold}${cyan}RISC-V Pipeline Simulator — MS4 Terminal GUI${reset}\n"
    line
    printf 'Input file:       %s\n' "${INPUT_FILE:-Not selected}"
    printf 'Branch predictor: %s\n' "$(predictor_name)"
    printf 'Data cache:       %s\n' "$(on_off "$ENABLE_CACHE")"
    printf 'Forwarding:       %s\n' "$(on_off "$ENABLE_FORWARDING")"
    printf 'Register dump:    %s\n' "$(on_off "$DUMP_REGISTERS")"
    printf 'Disassemble:      %s\n' "$(on_off "$DISASSEMBLE")"
    printf 'Init registers:   %s\n' "$(on_off "$INITIALIZE_REGISTERS")"
    printf 'Run until ecall:  %s\n' "$(on_off "$RUN_UNTIL_ECALL")"
    line
}

build_simulator() {
    printf "${yellow}Building simulator with BP_MODE=%s...${reset}\n" "$BP_MODE"
    make clean >/dev/null
    if make BP_MODE="$BP_MODE"; then
        printf "${green}Build successful.${reset}\n"
        return 0
    fi
    printf "${red}Build failed.${reset}\n"
    return 1
}

select_input_file() {
    clear_screen
    printf "${bold}Select an input program${reset}\n"
    line

    mapfile -t files < <(find code -type f -name '*.input' 2>/dev/null | sort)
    if [[ ${#files[@]} -eq 0 ]]; then
        printf "${red}No .input files were found under code/.${reset}\n"
        pause_screen
        return
    fi

    local i
    for i in "${!files[@]}"; do
        printf '%2d) %s\n' "$((i + 1))" "${files[$i]}"
    done
    printf ' 0) Cancel\n\nSelection: '
    read -r choice

    if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#files[@]} )); then
        INPUT_FILE="${files[$((choice - 1))]}"
    fi
}

select_predictor() {
    clear_screen
    printf "${bold}Select branch predictor${reset}\n"
    line
    printf '0) Disabled\n'
    printf '1) 1-bit last outcome\n'
    printf '2) 2-bit saturating counter\n\n'
    printf 'Selection: '
    read -r choice
    case "$choice" in
        0|1|2) BP_MODE="$choice" ;;
    esac
}

toggle_options() {
    while true; do
        show_header
        printf '1) Toggle data cache\n'
        printf '2) Toggle forwarding\n'
        printf '3) Toggle register dump\n'
        printf '4) Toggle disassembly\n'
        printf '5) Toggle register initialization\n'
        printf '6) Toggle run-until-ecall\n'
        printf '0) Return\n\nSelection: '
        read -r choice
        case "$choice" in
            1) ENABLE_CACHE=$((1 - ENABLE_CACHE)) ;;
            2) ENABLE_FORWARDING=$((1 - ENABLE_FORWARDING)) ;;
            3) DUMP_REGISTERS=$((1 - DUMP_REGISTERS)) ;;
            4) DISASSEMBLE=$((1 - DISASSEMBLE)) ;;
            5) INITIALIZE_REGISTERS=$((1 - INITIALIZE_REGISTERS)) ;;
            6) RUN_UNTIL_ECALL=$((1 - RUN_UNTIL_ECALL)) ;;
            0) return ;;
        esac
    done
}

run_simulator() {
    clear_screen
    if [[ -z "$INPUT_FILE" || ! -f "$INPUT_FILE" ]]; then
        printf "${red}Select a valid input file first.${reset}\n"
        pause_screen
        return
    fi

    build_simulator || { pause_screen; return; }

    local args=(-s)
    [[ "$ENABLE_CACHE" -eq 1 ]] && args+=(-c)
    [[ "$ENABLE_FORWARDING" -eq 1 ]] && args+=(-f)
    [[ "$DUMP_REGISTERS" -eq 1 ]] && args+=(-r)
    [[ "$DISASSEMBLE" -eq 1 ]] && args+=(-d)
    [[ "$INITIALIZE_REGISTERS" -eq 1 ]] && args+=(-v)
    [[ "$RUN_UNTIL_ECALL" -eq 1 ]] && args+=(-e)

    printf '\n'
    line
    printf "${bold}Running:${reset} ./riscv %s %s\n" "${args[*]}" "$INPUT_FILE"
    line
    ./riscv "${args[@]}" "$INPUT_FILE"
    local status=$?
    line
    if [[ $status -eq 0 ]]; then
        printf "${green}Simulator finished successfully.${reset}\n"
    else
        printf "${red}Simulator exited with status %d.${reset}\n" "$status"
    fi
    pause_screen
}

run_all_tests() {
    clear_screen
    printf "${bold}Running all MS4 tests${reset}\n"
    line
    chmod +x test_all_ms4.sh
    ./test_all_ms4.sh
    local status=$?
    line
    if [[ $status -eq 0 ]]; then
        printf "${green}All MS4 tests passed.${reset}\n"
    else
        printf "${red}One or more MS4 tests failed.${reset}\n"
    fi
    pause_screen
}

show_help() {
    clear_screen
    printf "${bold}Terminal GUI help${reset}\n"
    line
    cat <<'HELP'
This interface lets you operate the simulator without remembering commands.

1. Select an input file from the code/ directory.
2. Select predictor mode 0, 1, or 2.
3. Change simulator options if needed.
4. Choose Build and Run.
5. Use Run All MS4 Tests to demonstrate Parts 1–3.

The terminal GUI itself is MS4 Part 4. It requires only Bash and a normal
terminal. No Tkinter or other graphical libraries are used.
HELP
    pause_screen
}

self_check() {
    local failed=0
    [[ -f Makefile ]] || { echo 'Missing Makefile'; failed=1; }
    [[ -f riscv.c ]] || { echo 'Missing riscv.c'; failed=1; }
    [[ -d code ]] || { echo 'Missing code directory'; failed=1; }
    command -v make >/dev/null || { echo 'Missing make command'; failed=1; }
    command -v gcc >/dev/null || { echo 'Missing gcc command'; failed=1; }

    if [[ $failed -eq 0 ]]; then
        echo 'Terminal GUI self-check: PASSED'
        exit 0
    fi
    echo 'Terminal GUI self-check: FAILED'
    exit 1
}

main_menu() {
    find_default_input
    while true; do
        show_header
        printf '1) Select input file\n'
        printf '2) Select branch predictor\n'
        printf '3) Change simulator options\n'
        printf '4) Build and run simulator\n'
        printf '5) Run all MS4 tests\n'
        printf '6) Help\n'
        printf '0) Exit\n\nSelection: '
        read -r choice
        case "$choice" in
            1) select_input_file ;;
            2) select_predictor ;;
            3) toggle_options ;;
            4) run_simulator ;;
            5) run_all_tests ;;
            6) show_help ;;
            0) clear_screen; printf 'Goodbye.\n'; exit 0 ;;
            *) ;;
        esac
    done
}

if [[ "${1:-}" == "--check" ]]; then
    self_check
fi

main_menu
