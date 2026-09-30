from __future__ import annotations

import json
import unittest

from pydantic import ValidationError

from backend.schema import MeetingMinutes


def _example_minutes() -> dict[str, object]:
    return {
        "title": "BMS 개발 주간 회의",
        "date": "2026-09-29",
        "summary": "BMS 통신 문제 및 셀 밸런싱 진행 상황을 논의함.",
        "topics": [
            {
                "topic": "MC33774 통신",
                "discussion": "DADD enumeration 과정에서 응답 누락 문제를 확인함.",
            }
        ],
        "decisions": ["CAN FD Data Bitrate 설정을 다시 검증한다."],
        "action_items": [
            {
                "task": "MC33665 CAN FD 설정 확인",
                "owner": "유제환",
                "due_date": "2026-10-02",
            }
        ],
        "open_issues": ["MC33774 응답 누락 원인 분석 필요"],
    }


class MeetingMinutesSchemaTest(unittest.TestCase):
    def test_project_example_json_is_valid(self) -> None:
        example = _example_minutes()

        minutes = MeetingMinutes.model_validate_json(
            json.dumps(example, ensure_ascii=False)
        )

        self.assertEqual(minutes.model_dump(mode="json"), example)

    def test_every_top_level_field_is_required(self) -> None:
        for field_name in _example_minutes():
            with self.subTest(field=field_name):
                incomplete = _example_minutes()
                del incomplete[field_name]

                with self.assertRaises(ValidationError):
                    MeetingMinutes.model_validate(incomplete)

    def test_nested_fields_are_required(self) -> None:
        cases = (
            ("topics", "discussion"),
            ("action_items", "task"),
            ("action_items", "owner"),
            ("action_items", "due_date"),
        )

        for collection_name, field_name in cases:
            with self.subTest(collection=collection_name, field=field_name):
                incomplete = _example_minutes()
                collection = incomplete[collection_name]
                self.assertIsInstance(collection, list)
                del collection[0][field_name]  # type: ignore[index]

                with self.assertRaises(ValidationError):
                    MeetingMinutes.model_validate(incomplete)

    def test_unknown_date_owner_and_due_date_accept_null(self) -> None:
        example = _example_minutes()
        example["date"] = None
        action_items = example["action_items"]
        self.assertIsInstance(action_items, list)
        action_items[0]["owner"] = None  # type: ignore[index]
        action_items[0]["due_date"] = None  # type: ignore[index]

        minutes = MeetingMinutes.model_validate(example)

        self.assertIsNone(minutes.date)
        self.assertIsNone(minutes.action_items[0].owner)
        self.assertIsNone(minutes.action_items[0].due_date)

    def test_wrong_field_types_are_rejected_without_coercion(self) -> None:
        example = _example_minutes()
        example["decisions"] = "CAN FD 설정 확인"

        with self.assertRaises(ValidationError):
            MeetingMinutes.model_validate(example)

    def test_unknown_fields_are_rejected(self) -> None:
        example = _example_minutes()
        example["participants"] = ["유제환"]

        with self.assertRaises(ValidationError):
            MeetingMinutes.model_validate(example)


if __name__ == "__main__":
    unittest.main()
