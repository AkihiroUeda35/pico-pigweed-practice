from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class LedRequest(_message.Message):
    __slots__ = ("on",)
    ON_FIELD_NUMBER: _ClassVar[int]
    on: bool
    def __init__(self, on: bool = ...) -> None: ...

class LedResponse(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class EchoRequest(_message.Message):
    __slots__ = ("msg",)
    MSG_FIELD_NUMBER: _ClassVar[int]
    msg: str
    def __init__(self, msg: _Optional[str] = ...) -> None: ...

class EchoResponse(_message.Message):
    __slots__ = ("msg",)
    MSG_FIELD_NUMBER: _ClassVar[int]
    msg: str
    def __init__(self, msg: _Optional[str] = ...) -> None: ...
