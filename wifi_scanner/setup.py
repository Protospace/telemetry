from setuptools import setup

setup(
    name='wifi_scanner',
    packages=['wifi_scanner'],
    version='0.6.0',
    description='A tshark wrapper to count the number of cellphones in the vicinity',
    author='tannercollin',
    url='https://github.com/Protospace/telemetry',
    author_email='git@tannercollin.com',
    keywords=['tshark', 'wifi', 'location'],
    classifiers=[],
    install_requires=[
        'click',
        'netifaces',
        'pick',
        'requests',
    ],
    setup_requires=[],
    tests_require=[],
    entry_points={'console_scripts': [
        'wifi_scanner = wifi_scanner.__main__:main',
    ], },
)
