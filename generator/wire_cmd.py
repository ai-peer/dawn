# Copyright 2019 The Dawn Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from collections import namedtuple
from main import Name
from main import RecordMember
from main import Type
from main import NativeType

class CommandType(Type):
    def __init__(self, name, json_data, inputs=None, outputs=None):
        Type.__init__(self, name, json_data)
        self.inputs = inputs or []
        self.outputs = outputs or []
        self.derived_object = None
        self.derived_method = None

    @property
    def members(self):
        return self.inputs + self.outputs

# Create wire commands from api methods
def add_wire_commands(params):
    wire_params = params.copy()
    types = wire_params['types']

    string_message_member = RecordMember(Name('message'), types['char'], 'const*', False)
    string_message_member.length = 'strlen'

    # Generate commands from object methods
    for api_object in wire_params['by_category']['object']:
        for method in api_object.methods:
            if method.return_type.category != 'object' and method.return_type.name.canonical_case() != 'void':
                # No other return types supported
                continue

            inputs = []
            outputs = []

            # Create object method commands by prepending "self"
            inputs.append(RecordMember(Name('self'), types[api_object.dict_name], 'value', False))

            for arg in method.arguments:
                inputs.append(arg)

            # Client->Server commands that return an object return the result object handle
            if method.return_type.category == 'object':
                outputs.append(RecordMember(Name('result'), method.return_type, 'handle', False))

            name = ' '.join(api_object.name.chunks + method.name.chunks)
            command = CommandType(name, { 'category': 'command' }, inputs, outputs)

            command.derived_object = api_object
            command.derived_method = method
            wire_params['by_category']['command'].append(command)
            wire_params[name] = command

        # Create builder return ErrorCallback commands
        if api_object.is_builder:
            name = ' '.join(api_object.name.chunks + ['error callback'])

            command = CommandType(name, { 'category': 'return command' }, [
                RecordMember(Name('built object'), api_object.built_type, 'handle', False),
                RecordMember(Name("status"), types['uint32_t'], 'value', False),
                string_message_member,
            ])
            command.derived_object = api_object
            wire_params['by_category']['return command'].append(command)
            wire_params[name] = command

    wire_params['by_category']['command'].sort(key=lambda c: c.dict_name)
    wire_params['by_category']['return command'].sort(key=lambda c: c.dict_name)

    SerializationInfo = namedtuple('SerializationInfo', ['has_dawn_object'])
    wire_params['serialization_info'] = {}

    # Determine if a command contains a Dawn object. If it does not,
    # [de]serialization will not need an ObjectIdProvider/Resolver
    def has_dawn_object(type):
        for member in type.members:
            if member.type.category == 'object' and member.annotation != 'handle':
                return True
            elif member.type.category == 'structure' and has_dawn_object(member.type):
                return True
        return False

    for structure in wire_params['by_category']['structure']:
        wire_params['serialization_info'][structure.dict_name] = SerializationInfo(has_dawn_object(structure))

    for command in wire_params['by_category']['command']:
        wire_params['serialization_info'][command.dict_name] = SerializationInfo(has_dawn_object(command))

    for command in wire_params['by_category']['return command']:
        wire_params['serialization_info'][command.dict_name] = SerializationInfo(has_dawn_object(command))

    return wire_params
