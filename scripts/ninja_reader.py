#!/usr/bin/env python3
# Copyright 2026 dywoq - MIT License
# https://github.com/djeada/Standard-of-Iron

"""
A command-line interface development tool that is responsible for reading
".ninja_log" files, calculating the total and average compilation time and
printing the top slowest source files.
"""

import argparse
import datetime
import pathlib
from enum import Enum


class ElementParsingStage(Enum):
    """
    Identifies the current stage of parsing one of the ".ninja_log" elements.
    It is required to make investigating potential bugs much easier.
    """

    START_TIME = 0
    END_TIME = 1
    MODIFICATION_TIME = 2
    OUTPUT_FILE = 3

    def get_human_readable_str(self) -> str:
        """
        Converts this element parsing stage into a human-readable message
        that is placed at the end of error messages to report the source of
        a specific error.
        """
        if self == ElementParsingStage.START_TIME:
            return " when getting the start time"
        if self == ElementParsingStage.END_TIME:
            return " when getting the end time"
        if self == ElementParsingStage.MODIFICATION_TIME:
            return " when getting the modification time"
        if self == ElementParsingStage.OUTPUT_FILE:
            return " when getting the output file"
        return "none"


class NinjaLogElement:
    start_time = 0
    end_time = 0
    modification_time = 0
    output_file = ""

    def duration(self) -> int:
        """
        Substracts the start time from the end time, returning the result.
        """
        return self.end_time - self.start_time

    def parse_line(self, line: str):
        """
        Divides the supplied line into tab-separated columns, retrieving
        the information from each column. Following information specifies
        what each column contains and where its information is written to:

        1: Start time -> `self.start_time`

        2: End time -> `self.end_time`

        3: Modification Time -> `self.modification_time`

        4: Output File -> `self.output_file`

        5: Hash -> None (not used)
        """
        if len(line) == 0:
            raise Exception("Provided line is empty")

        def get_next_field(start_index: int, field_name: str, stage):
            next_tab_index = line.find("\t", start_index)

            if next_tab_index == -1:
                raise Exception(
                    f"Failed to get the starting tab character of "
                    f"the {field_name} {stage.get_human_readable_str()}"
                )

            return line[start_index:next_tab_index], next_tab_index + 1

        start = 0

        value, start = get_next_field(start, "end time", ElementParsingStage.START_TIME)
        self.start_time = int(value)

        value, start = get_next_field(
            start, "modification time", ElementParsingStage.END_TIME
        )
        self.end_time = int(value)

        value, start = get_next_field(
            start, "output file", ElementParsingStage.MODIFICATION_TIME
        )
        self.modification_time = value

        value, start = get_next_field(start, "hash", ElementParsingStage.OUTPUT_FILE)
        self.output_file = value


class NinjaLogParser:
    """
    Reads and parses a Ninja build log file into NinjaLogElement instances.
    The first line contains the log format version and is ignored.
    """

    def parse(self, path: pathlib.Path) -> list[NinjaLogElement]:
        elements = []
        with open(path) as file:
            next(file, None)
            for line in file:
                if not line.strip():
                    continue
                element = NinjaLogElement()
                element.parse_line(line.rstrip("\n"))
                elements.append(element)
        return elements


class SourceFilesSorter:
    """
    Sorts source files by compilation duration and keeps only the latest
    logged entry for each output file.
    """

    def sort(self, elements: list[NinjaLogElement]) -> list[NinjaLogElement]:
        latest_output_files = {}
        for element in elements:
            existing = latest_output_files.get(element.output_file)
            if (
                existing is None
                or element.modification_time > existing.modification_time
            ):
                latest_output_files[element.output_file] = element
        return sorted(
            latest_output_files.values(),
            key=lambda element: element.duration(),
            reverse=True,
        )


class SourceFilesCompilationTimeCalculator:
    """
    Calculates total and average compilation time across source files.
    """

    def calculate_total_time(self, elements: list[NinjaLogElement]) -> int:
        return sum(element.duration() for element in elements)

    def calculate_average_time(self, elements: list[NinjaLogElement]) -> float:
        if not elements:
            return 0
        return self.calculate_total_time(elements) / len(elements)


def get_arguments() -> argparse.Namespace:
    """
    Parses and returns the command-line arguments. If none arguments are
    provided, the function interrupts the program execution.
    """
    parser = argparse.ArgumentParser(
        description="Calculate the compilation time and print source files"
        + " taking the longest to compile"
    )
    parser.add_argument(
        "-t",
        "--top",
        type=int,
        default=20,
        help="Maximum count of slowest " + "source files to print",
    )
    parser.add_argument(
        "-b",
        "--build",
        type=str,
        required=True,
        help="Build directory " + " where the .ninja_log file is located",
    )
    return parser.parse_args()


def main():
    args = get_arguments()
    ninjalog_path = pathlib.Path(args.build) / ".ninja_log"

    ninjalog_parser = NinjaLogParser()
    source_files_sorter = SourceFilesSorter()
    compilation_time_calculator = SourceFilesCompilationTimeCalculator()

    try:
        elements = ninjalog_parser.parse(ninjalog_path)

        if not elements:
            print(f"No source files found in {ninjalog_path}")
            return -1

        sorted_elements = source_files_sorter.sort(elements)

        final_top = min(args.top, len(sorted_elements))
        if args.top > len(sorted_elements):
            print(f"(there are only {len(sorted_elements)} output files)")

        print(f"Top {final_top} slowest output files:\n")

        for element in sorted_elements[:final_top]:
            print(
                f"* {element.output_file}: "
                f"{datetime.timedelta(milliseconds=element.duration())}"
            )

        total_time = compilation_time_calculator.calculate_total_time(sorted_elements)
        average_time = compilation_time_calculator.calculate_average_time(
            sorted_elements
        )

        print(
            f"\nTotal compilation time: {datetime.timedelta(milliseconds=total_time)}"
        )
        print(
            f"Average compilation time: {datetime.timedelta(milliseconds=average_time)}"
        )

    except Exception as e:
        print(e)


if __name__ == "__main__":
    main()
