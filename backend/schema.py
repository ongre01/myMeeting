"""Validated data types for structured meeting-minutes JSON."""

from __future__ import annotations

from pydantic import BaseModel, ConfigDict


class _SchemaModel(BaseModel):
    """Common validation policy for data produced by the local LLM."""

    model_config = ConfigDict(extra="forbid", strict=True)


class Topic(_SchemaModel):
    """A discussion topic and its transcript-grounded description."""

    topic: str
    discussion: str


class ActionItem(_SchemaModel):
    """A follow-up task; unknown owners and dates are represented by null."""

    task: str
    owner: str | None
    due_date: str | None


class MeetingMinutes(_SchemaModel):
    """The complete structured result expected from meeting analysis."""

    title: str
    date: str | None
    summary: str
    topics: list[Topic]
    decisions: list[str]
    action_items: list[ActionItem]
    open_issues: list[str]


__all__ = ["ActionItem", "MeetingMinutes", "Topic"]
