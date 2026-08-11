import gitlint
from gitlint.rules import LineRule, RuleViolation, CommitMessageTitle

class UpperCaseType(LineRule):
    """ This rule will enforce that the commit message title use uppercase
    in the word between parenthesis. """

    name = "title-type-uppercase"
    id = "P1"
    target = CommitMessageTitle

    error_msg = ('Type word should be uppercase: '
                 '"{word}"')

    def validate(self, line, commit):
        # type: (Text, gitlint.commit) -> List[RuleViolation]
        violations = []

        type_word = line.split('(')[0]
        first_word = type_word

        if type_word.islower():
            violation = RuleViolation(self.id, self.error_msg.format(
                word=type_word.upper(),
            ))

            violations.append(violation)

            return violations

class LowerCaseType(LineRule):
    """ Enforce use of lower case if first outline word"""

    name = "outline-type-lowercase"
    id = "P2"
    target = CommitMessageTitle

    error_msg = ('First outline word should be lower case: '
                 '"{word}"')

    def validate(self, line, commit):
        # type: (Text, gitlint.commit) -> List[RuleViolation]
        violations = []

        index = line.find(":")
        outline_character = line[index+2:index+3]
        outline_word = line.split(" ")[1]

        if outline_character.isupper():
            violation = RuleViolation(self.id, self.error_msg.format(
                word=outline_word.lower(),

            ))

            violations.append(violation)

            return violations
